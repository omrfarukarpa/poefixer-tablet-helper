// Tablet Helper — PoeFixer plugin (SDK v6)
// Highlights Precursor Tablets across inventory, stash, special Tablet stash,
// guild stash and merchant windows, with per-type color + bonus/uses-left
// filtering.

#include "sdk/PluginSDK.h"

#include "config/Settings.h"
#include "game/HighlightRenderer.h"
#include "game/TabletBonusCatalog.h"
#include "game/TabletMatch.h"
#include "game/TabletRanges.h"
#include "game/TabletScanner.h"
#include "game/TabletTypes.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

inline constexpr const char* kTabletHelperVersion    = "1.3.1";
inline constexpr const char* kTabletHelperMaintainer = "Omer Faruk ARPA";

class TabletHelperPlugin : public PluginSDK::Plugin {
public:
    const char* GetName() const override { return "Tablet Helper"; }

    // The overlay must draw whenever the plugin is enabled and the overlay is
    // on; DrawUI is the host's per-frame overlay callback.
    bool WantsOverlay() const override {
        return m_settings.enabled && m_settings.overlayEnabled;
    }

    void OnEnable(bool /*isGameAttached*/) override {
        // On an ABI mismatch every service pointer is null; refuse to run rather
        // than silently no-op. Log is safe even then (wrappers null-check).
        if (!HostCompatible()) {
            ctx()->Log.Error(
                "Tablet Helper: incompatible PoeFixer host (SDK version/size mismatch) — disabled");
            return;
        }
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        m_settings.Load(DirectoryPath());
        ctx()->Log.Info(m_ranges.Load(DirectoryPath()).c_str());
        m_lastScan = std::chrono::steady_clock::now()
                     - std::chrono::milliseconds(m_settings.scanIntervalMs);
        ctx()->Log.Info("Tablet Helper plugin enabled");
    }

    void OnDisable() override {
        m_scanner.Reset();
        m_visible.clear();
        m_draw.clear();
        SaveSettings();
        ctx()->Log.Info("Tablet Helper plugin disabled");
    }

    void DrawUI() override {
        if (!m_settings.enabled || !m_settings.overlayEnabled) return;
        if (!ctx()->Game.IsInGame()) return;
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        // IsForeground() is O(1); GetSnapshot() would walk every world entity each
        // frame just to read this one flag.
        if (!ctx()->Game.IsForeground()) return;

        const ImVec2 disp = ImGui::GetIO().DisplaySize;
        RefreshIfNeeded(disp.x, disp.y);

        // Render the cached, pre-evaluated highlights (rebuilt only per scan).
        for (const auto& d : m_draw)
            TabletHelper::RenderTablet(d.rect, d.eval, m_settings);
    }

    void DrawSettings() override {
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        ImGui::TextDisabled("Tablet Helper v%s  -  by %s",
                            kTabletHelperVersion, kTabletHelperMaintainer);
        ImGui::Checkbox("Enable Tablet Helper", &m_settings.enabled);
        ImGui::SameLine();
        ImGui::Checkbox("Show overlay", &m_settings.overlayEnabled);

        if (ImGui::CollapsingHeader("How to use")) {
            ImGui::TextWrapped(
                "Tablet Helper highlights Precursor Tablets in any open item window "
                "(inventory, stash, special Tablet stash, guild stash, merchant).");
            ImGui::Spacing();
            ImGui::TextWrapped(
                "Each tablet type has its own color and filter below. Leave a type's "
                "bonus list empty to highlight ALL tablets of that type; pick bonuses "
                "to highlight only tablets that carry them. 'Global' applies across "
                "every type.");
            ImGui::Spacing();
            ImGui::TextDisabled(
                "Uses-left and bonus filtering require 'Read item mods' (below).");
        }

        DrawDisplaySettings();
        DrawDetectionSettings();
        DrawProfileSelector();

        ImGui::SeparatorText("Tablet types");
        for (auto& t : m_settings.ActiveTypes())
            DrawTypeConfig(t);

        DrawDebugSettings();
    }

    void SaveSettings() override { m_settings.Save(DirectoryPath()); }

private:
    struct DrawItem {
        TabletHelper::ScreenRect rect;
        TabletHelper::Evaluation eval;
    };

    TabletHelperConfig::Settings m_settings;
    TabletHelper::TabletScanner m_scanner;
    TabletHelper::TabletRanges m_ranges;
    std::vector<TabletHelper::VisibleTablet> m_visible;
    std::vector<DrawItem> m_draw;  // matched tablets + resolved color/labels, per scan
    std::chrono::steady_clock::time_point m_lastScan{};
    // Per-type bonus search text (settings-UI only; not persisted).
    std::unordered_map<std::string, std::string> m_bonusFilter;
    std::string m_debugStatus;
    int m_dumpCount = 0;

    void RefreshIfNeeded(float w, float h) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastScan).count();
        if (elapsed < m_settings.scanIntervalMs) return;
        m_lastScan = now;
        m_visible = m_scanner.Scan(ctx(), m_settings.readMods, w, h);

        // Evaluate matches ONCE per scan (not per render frame): resolve each
        // matched tablet's color + label lines and cache them for DrawUI.
        m_draw.clear();
        m_draw.reserve(m_visible.size());
        for (const auto& vt : m_visible) {
            if (!vt.parsed) continue;
            auto eval = TabletHelper::EvaluateTablet(m_settings, *vt.parsed);
            if (!eval.matched) continue;
            m_draw.push_back(DrawItem{vt.rect, std::move(eval)});
        }
    }

    void DrawDisplaySettings() {
        ImGui::SeparatorText("Display");

        int mode = static_cast<int>(m_settings.highlightMode);
        if (ImGui::Combo("Highlight style", &mode, "Border\0Cross\0Star\0"))
            m_settings.highlightMode = static_cast<TabletHelperConfig::HighlightMode>(mode);
        ImGui::SliderInt("Border/line thickness", &m_settings.borderThickness,
                         TabletHelperConfig::kBorderThicknessMin,
                         TabletHelperConfig::kBorderThicknessMax);

        ImGui::Checkbox("Show label", &m_settings.showLabel);
        if (m_settings.showLabel) {
            ImGui::Indent();
            ImGui::Checkbox("Show uses-left in label", &m_settings.showUsesLeft);
            ImGui::Checkbox("Show matched bonus names", &m_settings.showMatchedBonuses);
            int tmode = static_cast<int>(m_settings.labelTextMode);
            if (ImGui::Combo("Label text color", &tmode, "White\0Type color\0"))
                m_settings.labelTextMode = static_cast<TabletHelperConfig::LabelTextMode>(tmode);
            ImGui::Checkbox("Label outline", &m_settings.labelOutline);
            ImGui::Checkbox("Label background", &m_settings.labelBackground);
            if (m_settings.labelBackground)
                ImGui::SliderInt("Label bg alpha", &m_settings.labelBackgroundAlpha,
                                 TabletHelperConfig::kLabelBgAlphaMin,
                                 TabletHelperConfig::kLabelBgAlphaMax);
            ImGui::SliderFloat("Label scale", &m_settings.labelScale,
                               TabletHelperConfig::kLabelScaleMin,
                               TabletHelperConfig::kLabelScaleMax, "%.2f");
            ImGui::Unindent();
        }
    }

    void DrawDetectionSettings() {
        ImGui::SeparatorText("Detection");
        ImGui::Checkbox("Read item mods (uses-left + bonus filtering)", &m_settings.readMods);
        if (m_settings.readMods) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 190, 90, 255));
            ImGui::TextWrapped(
                "Reads each tablet's mods to compute uses-left and match bonuses. "
                "If tablets ever crash the game, turn this off (or set "
                "\"read_mods\": false in config/settings.json).");
            ImGui::PopStyleColor();
        } else {
            ImGui::TextDisabled("Mods off: tablets are highlighted by TYPE only "
                                "(uses-left and bonus filters are inactive).");
        }
        ImGui::SliderInt("Scan interval (ms)", &m_settings.scanIntervalMs,
                         TabletHelperConfig::kScanIntervalMinMs,
                         TabletHelperConfig::kScanIntervalMaxMs);
    }

    void DrawProfileSelector() {
        ImGui::SeparatorText("Filter profile");

        auto& profiles = m_settings.profiles;
        if (profiles.empty()) profiles.push_back(TabletHelperConfig::Profile{});
        int& active = m_settings.activeProfile;
        if (active < 0 || active >= static_cast<int>(profiles.size())) active = 0;

        ImGui::SetNextItemWidth(240.f);
        if (ImGui::BeginCombo("##profile", profiles[active].name.c_str())) {
            for (int i = 0; i < static_cast<int>(profiles.size()); ++i) {
                ImGui::PushID(i);
                const bool sel = (i == active);
                if (ImGui::Selectable(profiles[i].name.c_str(), sel)) active = i;
                if (sel) ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        HelpMarker("Each profile holds its own per-type colors, bonus selections "
                   "and uses-left filters. Switch to instantly re-filter for a "
                   "different strat. Display and detection settings above are "
                   "shared across all profiles.");

        // Rename the active profile in place.
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s", profiles[active].name.c_str());
            ImGui::SetNextItemWidth(240.f);
            if (ImGui::InputTextWithHint("Name", "profile name...", buf, sizeof(buf)))
                profiles[active].name = buf;
        }

        const int n = static_cast<int>(profiles.size());
        const bool atCap = n >= TabletHelperConfig::kMaxProfiles;

        if (atCap) ImGui::BeginDisabled();
        if (ImGui::Button("New")) {
            TabletHelperConfig::Profile p;
            p.name = "Profile " + std::to_string(n + 1);
            profiles.push_back(std::move(p));
            active = static_cast<int>(profiles.size()) - 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            TabletHelperConfig::Profile p = profiles[active];
            p.name += " copy";
            if (p.name.size() > TabletHelperConfig::kMaxProfileNameLen)
                p.name.resize(TabletHelperConfig::kMaxProfileNameLen);
            profiles.push_back(std::move(p));
            active = static_cast<int>(profiles.size()) - 1;
        }
        if (atCap) ImGui::EndDisabled();

        ImGui::SameLine();
        const bool onlyOne = n <= 1;
        if (onlyOne) ImGui::BeginDisabled();
        if (ImGui::Button("Delete")) {
            profiles.erase(profiles.begin() + active);
            if (profiles.empty()) profiles.push_back(TabletHelperConfig::Profile{});
            if (active >= static_cast<int>(profiles.size()))
                active = static_cast<int>(profiles.size()) - 1;
        }
        if (onlyOne) ImGui::EndDisabled();

        if (atCap)
            ImGui::TextDisabled("Profile limit reached (%d).",
                                TabletHelperConfig::kMaxProfiles);
    }

    // Inline min/max value fields for one bonus row (right of the "req" toggle).
    void DrawBonusValueFilter(TabletHelperConfig::TypeConfig& t, const TabletHelper::Bonus& b) {
        const TabletHelper::ModRange* rg = m_ranges.For(b.NormId);
        if (!rg && !b.NormIdStripped.empty()) rg = m_ranges.For(b.NormIdStripped);
        if (!rg || rg->min == rg->max) return;  // single flat value / no range

        auto& vr = t.valueFilters[b.Id];
        const int lo = rg->min < 0 ? rg->min : 0;
        const int hi = rg->max;
        const bool pct = rg->unit == "percent";

        ImGui::BeginDisabled(!m_settings.readMods);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(56.f);
        ImGui::DragInt("##vmin", &vr.min, 1.0f, lo, hi, "\xE2\x89\xA5%d");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Min value. Roll range %d-%d%s. 0 = no limit.",
                              rg->min, rg->max, pct ? "%%" : "");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(56.f);
        ImGui::DragInt("##vmax", &vr.max, 1.0f, lo, hi, "\xE2\x89\xA4%d");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Max value. Roll range %d-%d%s. 0 = no limit.",
                              rg->min, rg->max, pct ? "%%" : "");
        ImGui::EndDisabled();
    }

    void DrawDebugSettings() {
        if (!ImGui::CollapsingHeader("Debug: dump tablet mods")) return;
        ImGui::TextWrapped("Open each stash with tablets and press dump once per stash. "
                           "Each press APPENDS to one file (up to 40 tablets per press), so "
                           "you accumulate every tablet across stashes.");

        const auto path = DirectoryPath() / "config" / "tablet_debug_dump.txt";
        if (ImGui::Button("Scan & dump tablet mods")) {
            const std::string dump = m_scanner.BuildDebugDump(ctx());
            std::error_code ec;
            std::filesystem::create_directories(DirectoryPath() / "config", ec);
            std::ofstream out(path, std::ios::app);
            if (out.is_open()) {
                out << "\n===== DUMP #" << ++m_dumpCount << " =====\n" << dump;
                m_debugStatus = "Appended dump #" + std::to_string(m_dumpCount)
                                + " -> " + path.string();
            } else {
                m_debugStatus = "Failed to write dump file.";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear dump file")) {
            std::ofstream out(path, std::ios::trunc);
            m_dumpCount = 0;
            m_debugStatus = "Cleared " + path.string();
        }
        if (!m_debugStatus.empty()) ImGui::TextWrapped("%s", m_debugStatus.c_str());
    }

    static bool Contains(const std::vector<std::string>& v, const std::string& id) {
        for (const auto& s : v) if (s == id) return true;
        return false;
    }

    static void RemoveFrom(std::vector<std::string>& v, const std::string& id) {
        v.erase(std::remove(v.begin(), v.end(), id), v.end());
    }

    static void HelpMarker(const char* desc) {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", desc);
    }

    void DrawTypeConfig(TabletHelperConfig::TypeConfig& t) {
        ImGui::PushID(t.key.c_str());

        const int selCount = static_cast<int>(t.selectedBonusIds.size());
        const int reqCount = static_cast<int>(t.requiredBonusIds.size());
        const int optCount = selCount - reqCount;
        char header[112];
        if (reqCount > 0)
            std::snprintf(header, sizeof(header), "%s  (%d req + %d opt)###hdr",
                          t.displayName.c_str(), reqCount, optCount);
        else if (selCount > 0)
            std::snprintf(header, sizeof(header), "%s  (%d bonus%s)###hdr",
                          t.displayName.c_str(), selCount, selCount == 1 ? "" : "es");
        else
            std::snprintf(header, sizeof(header), "%s###hdr", t.displayName.c_str());

        if (ImGui::CollapsingHeader(header)) {
            ImGui::Indent();
            ImGui::Checkbox("Enabled", &t.enabled);
            ImGui::ColorEdit4("Color", t.color,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::SliderInt("Min uses-left", &t.minUsesLeft, 0, TabletHelperConfig::kMinUsesMax);
            ImGui::SameLine();
            HelpMarker("Only highlight tablets with at least this many remaining uses "
                       "(the tablet's 'adds N uses' implicit). Requires 'Read item mods'. "
                       "0 = no uses requirement.");
            if (t.minUsesLeft > 0 && !m_settings.readMods) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 150, 90, 255));
                ImGui::TextWrapped("Read item mods is OFF, so uses-left reads 0 and this "
                                   "hides every %s. Turn mods on or set this to 0.",
                                   TabletHelper::ShortName(t.key));
                ImGui::PopStyleColor();
            }

            const auto& bonuses = TabletHelper::GetBonusesFor(t.key);
            if (selCount == 0) {
                ImGui::TextDisabled("No bonus filter: every %s is highlighted.",
                                    TabletHelper::ShortName(t.key));
            } else {
                auto poolSlider = [](const char* label, int* val, int poolTotal) {
                    int cap = poolTotal < TabletHelperConfig::kMinMatchedMax
                                  ? poolTotal : TabletHelperConfig::kMinMatchedMax;
                    if (cap < 1) cap = 1;
                    if (*val > cap) *val = cap;
                    if (*val < 1) *val = 1;
                    ImGui::SliderInt(label, val, 1, cap);
                };
                if (reqCount > 0) {
                    poolSlider("Min required mods", &t.minRequiredBonuses, reqCount);
                    ImGui::SameLine();
                    HelpMarker("How many of the REQUIRED bonuses (marked 'req') must be "
                               "present. Set it to the required count to demand all of them.");
                }
                if (optCount > 0) {
                    poolSlider("Min optional mods", &t.minMatchedBonuses, optCount);
                    ImGui::SameLine();
                    HelpMarker("How many of the remaining (optional) selected bonuses "
                               "must be present.");
                }
            }

            ImGui::Text("Match bonuses (%zu available):", bonuses.size());
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear selected")) {
                t.selectedBonusIds.clear();
                t.requiredBonusIds.clear();
            }
            ImGui::TextDisabled("Tick a bonus, then 'req' to make it required.");
            if (!m_settings.readMods)
                ImGui::TextDisabled("(bonus matching needs 'Read item mods' ON)");

            // Search box — stays above the scroll region so it doesn't scroll away.
            std::string& filter = m_bonusFilter[t.key];
            {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%s", filter.c_str());
                ImGui::SetNextItemWidth(-1.f);
                if (ImGui::InputTextWithHint("##bonusfilter", "type to filter bonuses...",
                                             buf, sizeof(buf)))
                    filter = buf;
            }
            const std::string lf = TabletHelper::ToLowerCopy(filter);

            ImGui::BeginChild("bonuslist", ImVec2(0.f, 170.f), ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_HorizontalScrollbar);
            std::string lastCategory;
            int shown = 0;
            for (const auto& b : bonuses) {
                if (!lf.empty() && !TabletHelper::ContainsCI(b.Label, lf)
                    && !TabletHelper::ContainsCI(b.Id, lf))
                    continue;
                if (b.Category != lastCategory) {
                    lastCategory = b.Category;
                    ImGui::SeparatorText(b.Category.c_str());
                }
                bool sel = Contains(t.selectedBonusIds, b.Id);
                bool req = Contains(t.requiredBonusIds, b.Id);
                ImGui::PushID(b.Id.c_str());
                if (ImGui::Checkbox(b.Label.c_str(), &sel)) {
                    if (sel) {
                        if (!Contains(t.selectedBonusIds, b.Id))
                            t.selectedBonusIds.push_back(b.Id);
                    } else {
                        RemoveFrom(t.selectedBonusIds, b.Id);
                        RemoveFrom(t.requiredBonusIds, b.Id);  // unselected -> not required
                    }
                }
                if (sel) {
                    ImGui::SameLine();
                    if (ImGui::Checkbox("req", &req)) {
                        if (req) {
                            if (!Contains(t.requiredBonusIds, b.Id))
                                t.requiredBonusIds.push_back(b.Id);
                        } else {
                            RemoveFrom(t.requiredBonusIds, b.Id);
                        }
                    }
                    DrawBonusValueFilter(t, b);
                }
                ImGui::PopID();
                ++shown;
            }
            if (shown == 0)
                ImGui::TextDisabled("No bonuses match \"%s\".", filter.c_str());
            ImGui::EndChild();
            ImGui::Unindent();
        }

        ImGui::PopID();
    }
};

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() { return new TabletHelperPlugin(); }

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* p) { delete p; }
