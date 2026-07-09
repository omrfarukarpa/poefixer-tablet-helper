#pragma once

// Tablet discovery + per-item parse cache.
//
// Every scan: request a host refresh, enumerate all inventories, keep the items
// that (a) look like a Precursor Tablet by their pre-materialized Path/BaseType
// (crash-free) and (b) resolve to an on-screen rect. Each unique tablet (by
// entity address) is parsed at most once and cached; parsing optionally reads
// item mods to derive uses-left and the bonus match-key set.
//
// MOD-READ SAFETY: reading mods is the one risky host call (a wrong entity
// address can fault the host uncatchably — try/catch cannot save us from an
// access violation). Mitigations here: we only ever attempt it on confirmed
// tablets (never arbitrary items), we probe the address with the cheaper
// ReadItemBaseTypeName first, and we parse once per address and cache. The
// caller's readMods flag is the master off-switch if it ever misbehaves.

#include "PanelDetector.h"
#include "TabletTypes.h"
#include "sdk/PluginSDK.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace TabletHelper {

struct ParsedTablet {
    std::string typeKey = TypeKeys::Unknown;
    std::string sourcePath;  // guards against the game reusing an entity address
    int usesLeft = 0;
    std::unordered_set<std::string> matchKeys;  // normalized mod Name/Id (+stripped)
    std::unordered_map<std::string, int> matchValues;  // same keys -> rounded Value0
    bool modsRead = false;
};

struct VisibleTablet {
    std::uintptr_t address = 0;
    ScreenRect rect;
    const ParsedTablet* parsed = nullptr;  // stable: points into m_cache (node-based)
};

inline void AddMatchKey(std::unordered_set<std::string>& keys, const std::string& raw) {
    std::string n = NormalizeIdentifier(raw);
    if (n.empty()) return;
    keys.insert(n);
    std::string s = StripTrailingDigits(n);
    if (!s.empty() && s != n) keys.insert(s);
}

inline void AddMatchValue(std::unordered_map<std::string, int>& vals,
                          const std::string& raw, float value) {
    std::string n = NormalizeIdentifier(raw);
    if (n.empty()) return;
    const int iv = static_cast<int>(std::lround(value));
    vals[n] = iv;
    std::string s = StripTrailingDigits(n);
    if (!s.empty() && s != n) vals[s] = iv;
}

class TabletScanner {
public:
    std::vector<VisibleTablet> Scan(const PluginSDK::Context* ctx, bool readMods,
                                    float displayW, float displayH,
                                    int maxNewParsesPerScan = 40) {
        std::vector<VisibleTablet> out;
        if (!ctx) return out;

        // Guard against unbounded growth over a very long session.
        if (m_cache.size() > 4000) m_cache.clear();

        ctx->Inventory.Scan(-1);
        const auto all = ctx->Inventory.GetAll();

        std::unordered_set<std::uintptr_t> seenThisScan;
        int newParses = 0;

        for (const auto& inv : all) {
            for (const auto& item : inv.Items) {
                if (!LooksLikeTablet(item.Path, item.BaseTypeName)) continue;
                auto rect = ResolveItemRect(inv, item, displayW, displayH);
                if (!rect) continue;                 // not laid out on screen
                if (item.Address == 0) continue;     // need a stable cache key
                if (!seenThisScan.insert(item.Address).second) continue;

                auto it = m_cache.find(item.Address);
                const bool freshAddress =
                    it == m_cache.end() || it->second.sourcePath != item.Path;

                if (freshAddress) {
                    const bool doMods = readMods && newParses < maxNewParsesPerScan;
                    ParsedTablet p = Parse(ctx, item, doMods);
                    if (doMods && p.modsRead) ++newParses;
                    if (it == m_cache.end())
                        it = m_cache.emplace(item.Address, std::move(p)).first;
                    else
                        it->second = std::move(p);
                } else if (readMods && !it->second.modsRead
                           && newParses < maxNewParsesPerScan) {
                    // Parsed earlier without mods (readMods was off / capped);
                    // upgrade it now that we can read.
                    ParsedTablet p = Parse(ctx, item, true);
                    if (p.modsRead) { ++newParses; it->second = std::move(p); }
                }

                VisibleTablet vt;
                vt.address = item.Address;
                vt.rect = *rect;
                vt.parsed = &it->second;
                out.push_back(vt);
            }
        }
        return out;
    }

    void Reset() { m_cache.clear(); }

    // Fresh uncached dump of on-screen tablets' raw mods, for confirming internal
    // ids/Value0 against the bundled range data. Probe-first, confirmed tablets
    // only, capped — same crash-safe stance as Scan.
    std::string BuildDebugDump(const PluginSDK::Context* ctx, int maxItems = 40) {
        std::string s = "[Tablet Helper] mod dump\n";
        if (!ctx) { s += "no context\n"; return s; }

        ctx->Inventory.Scan(-1);
        const auto all = ctx->Inventory.GetAll();
        char line[600];
        int dumped = 0;

        for (const auto& inv : all) {
            for (const auto& item : inv.Items) {
                if (dumped >= maxItems) { s += "\n...(item cap reached)\n"; return s; }
                if (!LooksLikeTablet(item.Path, item.BaseTypeName)) continue;
                if (item.Address == 0) continue;
                if (ctx->Inventory.ReadItemBaseTypeName(item.Address).empty()) continue;

                const auto mods = ctx->Inventory.ReadItemMods(item.Address);
                std::snprintf(line, sizeof(line), "\n#%d base='%s' path='%s' valid=%d\n",
                              dumped + 1, item.BaseTypeName.c_str(), item.Path.c_str(),
                              mods.Valid ? 1 : 0);
                s += line;

                auto group = [&](const char* tag, const std::vector<PluginSDK::Mod>& g) {
                    for (const auto& m : g) {
                        std::string fmt = ctx->Inventory.FormatStat(m.StatKey, m.Value0, m.Value1);
                        std::snprintf(line, sizeof(line),
                            "  [%s] id='%s' name='%s' stat='%s' v0=%.2f v1=%.2f :: %s\n",
                            tag, m.Id.c_str(), m.Name.c_str(), m.StatKey.c_str(),
                            m.Value0, m.Value1, fmt.c_str());
                        s += line;
                    }
                };
                group("impl", mods.ImplicitMods);
                group("expl", mods.ExplicitMods);

                const auto agg = ctx->Inventory.ReadItemAggregatedStats(item.Address);
                std::snprintf(line, sizeof(line), "  aggregated: %zu pairs\n", agg.size());
                s += line;
                for (const auto& pr : agg) {
                    std::snprintf(line, sizeof(line), "    key=%d value=%d\n", pr.first, pr.second);
                    s += line;
                }
                ++dumped;
            }
        }
        if (dumped == 0) s += "no readable tablets on screen.\n";
        return s;
    }

private:
    ParsedTablet Parse(const PluginSDK::Context* ctx,
                       const PluginSDK::InventoryItem& item, bool readMods) {
        ParsedTablet p;
        p.sourcePath = item.Path;
        p.typeKey = ClassifyFromText(item.Path, item.BaseTypeName);

        if (readMods && item.Address) {
            // Lower-risk probe: touching the same entity address via the base-type
            // read exercises a dereference before the heavier mod read. A non-empty
            // result means the address is live; then read the mods.
            std::string probe = ctx->Inventory.ReadItemBaseTypeName(item.Address);
            if (!probe.empty()) {
                const auto mods = ctx->Inventory.ReadItemMods(item.Address);
                p.modsRead = mods.Valid;

                // Uses-left = sum of Value0 over the "adds N uses" IMPLICIT mods.
                // The "TowerAdd" token may live in Name OR Id (Mods.dat Id) on
                // PoeFixer, so check both. Restricting to the implicit bucket is
                // what keeps the explicit "TowerAdditional..." map bonuses (which
                // also contain the substring "toweradd") from being counted.
                for (const auto& m : mods.ImplicitMods) {
                    if (ContainsCI(m.Name, "toweradd") || ContainsCI(m.Id, "toweradd")) {
                        int v = static_cast<int>(m.Value0 + 0.5f);
                        if (v > 0) p.usesLeft += v;
                    }
                }

                std::string implicitText;  // for the classification fallback
                auto addGroup = [&](const std::vector<PluginSDK::Mod>& g, bool implicit) {
                    for (const auto& m : g) {
                        AddMatchKey(p.matchKeys, m.Name);
                        AddMatchKey(p.matchKeys, m.Id);
                        AddMatchValue(p.matchValues, m.Name, m.Value0);
                        AddMatchValue(p.matchValues, m.Id, m.Value0);
                        if (implicit) {
                            implicitText += ToLowerCopy(m.Name);
                            implicitText += ' ';
                            implicitText += ToLowerCopy(m.Id);
                            implicitText += ' ';
                            implicitText += ToLowerCopy(m.StatKey);
                            implicitText += ' ';
                        }
                    }
                };
                addGroup(mods.ImplicitMods, true);
                addGroup(mods.ExplicitMods, false);
                addGroup(mods.EnchantMods, false);
                addGroup(mods.HellscapeMods, false);
                addGroup(mods.CrucibleMods, false);

                if (p.typeKey == std::string(TypeKeys::Unknown)) {
                    const char* fromMods = ClassifyFromModText(implicitText);
                    if (std::string(fromMods) != TypeKeys::Unknown) p.typeKey = fromMods;
                }
            }
        }
        return p;
    }

    std::unordered_map<std::uintptr_t, ParsedTablet> m_cache;
};

}  // namespace TabletHelper
