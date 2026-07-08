#pragma once

// Tablet Helper settings — crash-safe JSON, mirroring QuickStash's Settings
// pattern (non-throwing parse, try/catch(...) around the whole body, clamps that
// share a single source of truth with the DrawSettings sliders). Load runs from
// OnEnable, so an exception escaping across the C ABI would crash the host.
//
// The config model is the "simplified per-type" one: one block per tablet type
// (plus a Global cross-type block) with enable / color / min-uses-left / a Match
// bonus multi-select — not the original's multi-group-per-type editor.

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

    std::vector<TypeConfig> types = DefaultTypes();

    TypeConfig* FindType(const std::string& key) {
        for (auto& t : types) if (t.key == key) return &t;
        return nullptr;
    }
    const TypeConfig* FindType(const std::string& key) const {
        for (const auto& t : types) if (t.key == key) return &t;
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

            // Rebuild from canonical defaults, then overlay saved per-type values
            // matched by key. This keeps the type list fixed (not user-editable)
            // and the display names/order canonical, exactly like EnsureDefaults.
            types = DefaultTypes();
            if (j.contains("types") && j["types"].is_array()) {
                for (const auto& e : j["types"]) {
                    if (!e.is_object() || !e.contains("key") || !e["key"].is_string())
                        continue;
                    TypeConfig* t = FindType(e["key"].get<std::string>());
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
            j["types"] = std::move(arr);

            const std::string text = j.dump(2);  // serialize BEFORE opening/truncating
            std::ofstream out(SettingsPath(pluginDir));
            if (out.is_open()) out << text;
        } catch (...) {
            // Disk/serialization failure must not propagate across the C ABI.
        }
    }
};

}  // namespace TabletHelperConfig
