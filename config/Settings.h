#pragma once

// Tablet Helper settings — crash-safe JSON (non-throwing parse, try/catch(...)
// around the whole body, clamps that share a single source of truth with the
// DrawSettings sliders). Load runs from OnEnable, so an exception escaping across
// the C ABI would crash the host.
//
// The config model is one block per tablet type (plus a Global cross-type block)
// with enable / color / min-uses-left / a Match bonus multi-select.

#include "../game/TabletTypes.h"
#include "../third_party/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace TabletHelperConfig {

// Slider bounds — the single source of truth shared by DrawSettings and Load.
inline constexpr int   kBorderThicknessMin = 1;
inline constexpr int   kBorderThicknessMax = 8;
inline constexpr float kLabelScaleMin      = 0.5f;
inline constexpr float kLabelScaleMax      = 2.0f;
inline constexpr int   kLabelBgAlphaMin    = 0;
inline constexpr int   kLabelBgAlphaMax    = 255;
inline constexpr int   kScanIntervalMinMs  = 50;
inline constexpr int   kScanIntervalMaxMs  = 1000;
inline constexpr int   kMinUsesMax         = 10;   // slider ceiling for min uses-left
inline constexpr int   kMinMatchedMax      = 8;    // slider ceiling for min matched bonuses

enum class HighlightMode { Border = 0, Cross = 1, Star = 2 };
enum class LabelTextMode { White = 0, TypeColor = 1 };

// Per-tablet-type (or Global) highlight configuration.
struct TypeConfig {
    std::string key;          // TabletHelper::TypeKeys::* (JSON identity — ASCII)
    std::string displayName;  // canonical, never persisted as authoritative
    bool  enabled = true;
    float color[4] = {0.0f, 0.749f, 1.0f, 1.0f};
    int   minUsesLeft = 0;
    int   minMatchedBonuses = 1;
    std::vector<std::string> selectedBonusIds;  // Match set; empty = match by type only
};

inline std::vector<TypeConfig> DefaultTypes() {
    using namespace TabletHelper;
    auto make = [](const char* key, float r, float g, float b, bool enabled) {
        TypeConfig t;
        t.key = key;
        t.displayName = DisplayName(key);
        t.enabled = enabled;
        t.color[0] = r; t.color[1] = g; t.color[2] = b; t.color[3] = 1.0f;
        return t;
    };
    // Distinct default colors so the out-of-the-box "highlight everything by
    // type" view is legible. Global is opt-in (disabled) so it doesn't blanket-
    // match every tablet before the user configures a cross-type filter.
    return {
        make(TypeKeys::Irradiated, 0.85f, 0.85f, 0.85f, true),
        make(TypeKeys::Breach,     0.65f, 0.30f, 0.90f, true),
        make(TypeKeys::Delirium,   0.20f, 0.80f, 0.80f, true),
        make(TypeKeys::Abyss,      0.80f, 0.15f, 0.15f, true),
        make(TypeKeys::Ritual,     0.95f, 0.35f, 0.20f, true),
        make(TypeKeys::Overseer,   0.95f, 0.80f, 0.20f, true),
        make(TypeKeys::Temple,     0.30f, 0.80f, 0.30f, true),
        make(TypeKeys::Global,     0.00f, 0.749f, 1.00f, false),
    };
}

inline TypeConfig* FindTypeIn(std::vector<TypeConfig>& types, const std::string& key) {
    for (auto& t : types) if (t.key == key) return &t;
    return nullptr;
}

// Overlay saved per-type values (matched by key) onto a fresh DefaultTypes()
// vector: the type list/order/display-names stay canonical, only the mutable
// fields are taken from JSON, each clamped to its slider bounds. Shared by the
// per-profile load path and the v1.0 single-config migration.
inline void ApplyTypeOverrides(std::vector<TypeConfig>& types, const nlohmann::json& arr) {
    if (!arr.is_array()) return;
    for (const auto& e : arr) {
        if (!e.is_object() || !e.contains("key") || !e["key"].is_string())
            continue;
        TypeConfig* t = FindTypeIn(types, e["key"].get<std::string>());
        if (!t) continue;
        if (e.contains("enabled") && e["enabled"].is_boolean())
            t->enabled = e["enabled"].get<bool>();
        if (e.contains("min_uses_left") && e["min_uses_left"].is_number_integer())
            t->minUsesLeft = std::clamp(e["min_uses_left"].get<int>(), 0, kMinUsesMax);
        if (e.contains("min_matched_bonuses") && e["min_matched_bonuses"].is_number_integer())
            t->minMatchedBonuses = std::clamp(
                e["min_matched_bonuses"].get<int>(), 1, kMinMatchedMax);
        if (e.contains("color") && e["color"].is_array()) {
            const auto& c = e["color"];
            for (int i = 0; i < 4 && i < static_cast<int>(c.size()); ++i)
                if (c[i].is_number())
                    t->color[i] = std::clamp(c[i].get<float>(), 0.0f, 1.0f);
        }
        if (e.contains("selected_bonus_ids") && e["selected_bonus_ids"].is_array()) {
            t->selectedBonusIds.clear();
            for (const auto& id : e["selected_bonus_ids"]) {
                if (!id.is_string()) continue;
                std::string s = id.get<std::string>();
                if (std::find(t->selectedBonusIds.begin(),
                              t->selectedBonusIds.end(), s)
                    == t->selectedBonusIds.end())
                    t->selectedBonusIds.push_back(std::move(s));
            }
        }
    }
}

inline nlohmann::json SerializeTypes(const std::vector<TypeConfig>& types) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& t : types) {
        nlohmann::json e;
        e["key"] = t.key;
        e["enabled"] = t.enabled;
        e["color"] = {t.color[0], t.color[1], t.color[2], t.color[3]};
        e["min_uses_left"] = t.minUsesLeft;
        e["min_matched_bonuses"] = t.minMatchedBonuses;
        e["selected_bonus_ids"] = t.selectedBonusIds;
        arr.push_back(std::move(e));
    }
    return arr;
}

// Slot-based profile limits (UI + JSON sanity).
inline constexpr int         kMaxProfiles       = 32;
inline constexpr std::size_t kMaxProfileNameLen = 63;

// A named filter profile: a full per-type + Global config set, switchable so the
// user can keep several strat-specific bonus/color setups instead of being locked
// to a single combination per tablet type.
struct Profile {
    std::string name = "Default";
    std::vector<TypeConfig> types = DefaultTypes();
};

struct Settings {
    bool  enabled = true;
    bool  overlayEnabled = true;
    bool  showLabel = true;
    bool  showUsesLeft = false;
    bool  showMatchedBonuses = false;
    HighlightMode highlightMode = HighlightMode::Border;
    LabelTextMode labelTextMode = LabelTextMode::TypeColor;
    bool  labelBackground = true;
    bool  labelOutline = true;
    int   labelBackgroundAlpha = 170;
    int   borderThickness = 3;
    float labelScale = 1.0f;
    int   scanIntervalMs = 150;
    // Master toggle for reading item mods. Uses-left and bonus matching depend
    // on it. It is behind a safe probe (see TabletScanner) but a bad entity
    // address can still fault the host uncatchably, so this can be turned off —
    // in-game via the checkbox, or by editing settings.json if it ever crashes.
    bool  readMods = true;

    // At least one profile always exists (guaranteed by the defaults here and by
    // Load); the active profile supplies the per-type config the rest of the
    // plugin reads through ActiveTypes()/FindType().
    std::vector<Profile> profiles = { Profile{} };
    int activeProfile = 0;

    std::vector<TypeConfig>& ActiveTypes() {
        if (profiles.empty()) profiles.push_back(Profile{});
        if (activeProfile < 0 || activeProfile >= static_cast<int>(profiles.size()))
            activeProfile = 0;
        return profiles[activeProfile].types;
    }
    const std::vector<TypeConfig>& ActiveTypes() const {
        const int idx =
            (activeProfile >= 0 && activeProfile < static_cast<int>(profiles.size()))
                ? activeProfile : 0;
        return profiles[idx].types;  // profiles is never empty
    }

    TypeConfig* FindType(const std::string& key) {
        for (auto& t : ActiveTypes()) if (t.key == key) return &t;
        return nullptr;
    }
    const TypeConfig* FindType(const std::string& key) const {
        for (const auto& t : ActiveTypes()) if (t.key == key) return &t;
        return nullptr;
    }

    std::filesystem::path SettingsPath(const std::filesystem::path& pluginDir) const {
        return pluginDir / "config" / "settings.json";
    }

    void Load(const std::filesystem::path& pluginDir) {
        try {
            const auto path = SettingsPath(pluginDir);
            if (!std::filesystem::exists(path)) return;
            std::ifstream in(path);
            if (!in.is_open()) return;

            nlohmann::json j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
            if (j.is_discarded() || !j.is_object()) return;

            enabled = j.value("enabled", enabled);
            overlayEnabled = j.value("overlay_enabled", overlayEnabled);
            showLabel = j.value("show_label", showLabel);
            showUsesLeft = j.value("show_uses_left", showUsesLeft);
            showMatchedBonuses = j.value("show_matched_bonuses", showMatchedBonuses);
            labelBackground = j.value("label_background", labelBackground);
            labelOutline = j.value("label_outline", labelOutline);
            readMods = j.value("read_mods", readMods);

            labelBackgroundAlpha = std::clamp(
                j.value("label_background_alpha", labelBackgroundAlpha),
                kLabelBgAlphaMin, kLabelBgAlphaMax);
            borderThickness = std::clamp(j.value("border_thickness", borderThickness),
                                         kBorderThicknessMin, kBorderThicknessMax);
            labelScale = std::clamp(j.value("label_scale", labelScale),
                                    kLabelScaleMin, kLabelScaleMax);
            scanIntervalMs = std::clamp(j.value("scan_interval_ms", scanIntervalMs),
                                        kScanIntervalMinMs, kScanIntervalMaxMs);

            highlightMode = static_cast<HighlightMode>(
                std::clamp(j.value("highlight_mode", static_cast<int>(highlightMode)), 0, 2));
            labelTextMode = static_cast<LabelTextMode>(
                std::clamp(j.value("label_text_mode", static_cast<int>(labelTextMode)), 0, 1));

            // Profiles: the schema is an array of {name, types:[...]}, each type
            // list rebuilt from canonical DefaultTypes() then overlaid by key.
            // Back-compat: a v1.0 file has a root-level "types" array and no
            // "profiles" — migrate it into a single "Default" profile. Always end
            // with at least one profile so ActiveTypes() has something to return.
            profiles.clear();
            if (j.contains("profiles") && j["profiles"].is_array()) {
                for (const auto& pj : j["profiles"]) {
                    if (!pj.is_object()) continue;
                    Profile p;
                    if (pj.contains("name") && pj["name"].is_string()) {
                        p.name = pj["name"].get<std::string>();
                        if (p.name.size() > kMaxProfileNameLen)
                            p.name = p.name.substr(0, kMaxProfileNameLen);
                    }
                    p.types = DefaultTypes();
                    if (pj.contains("types")) ApplyTypeOverrides(p.types, pj["types"]);
                    profiles.push_back(std::move(p));
                    if (static_cast<int>(profiles.size()) >= kMaxProfiles) break;
                }
            } else if (j.contains("types")) {
                Profile p;  // migrate v1.0 single-config schema
                p.types = DefaultTypes();
                ApplyTypeOverrides(p.types, j["types"]);
                profiles.push_back(std::move(p));
            }
            if (profiles.empty()) profiles.push_back(Profile{});
            activeProfile = std::clamp(j.value("active_profile", 0),
                                       0, static_cast<int>(profiles.size()) - 1);
        } catch (...) {
            // Corrupt/unexpected file — keep defaults, never propagate.
        }
    }

    void Save(const std::filesystem::path& pluginDir) const {
        try {
            std::error_code ec;
            std::filesystem::create_directories(pluginDir / "config", ec);

            nlohmann::json j;
            j["enabled"] = enabled;
            j["overlay_enabled"] = overlayEnabled;
            j["show_label"] = showLabel;
            j["show_uses_left"] = showUsesLeft;
            j["show_matched_bonuses"] = showMatchedBonuses;
            j["label_background"] = labelBackground;
            j["label_outline"] = labelOutline;
            j["read_mods"] = readMods;
            j["label_background_alpha"] = labelBackgroundAlpha;
            j["border_thickness"] = borderThickness;
            j["label_scale"] = labelScale;
            j["scan_interval_ms"] = scanIntervalMs;
            j["highlight_mode"] = static_cast<int>(highlightMode);
            j["label_text_mode"] = static_cast<int>(labelTextMode);

            nlohmann::json parr = nlohmann::json::array();
            for (const auto& p : profiles) {
                nlohmann::json pj;
                pj["name"] = p.name;
                pj["types"] = SerializeTypes(p.types);
                parr.push_back(std::move(pj));
            }
            j["profiles"] = std::move(parr);
            j["active_profile"] = activeProfile;

            const std::string text = j.dump(2);  // serialize BEFORE opening/truncating
            std::ofstream out(SettingsPath(pluginDir));
            if (out.is_open()) out << text;
        } catch (...) {
            // Disk/serialization failure must not propagate across the C ABI.
        }
    }
};

}  // namespace TabletHelperConfig
