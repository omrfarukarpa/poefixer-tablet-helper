#pragma once

// Loads config/tablet_mod_ranges.json (min/max/unit per mod id) for the value
// filter's slider bounds. Crash-safe, non-throwing; missing/corrupt file just
// disables value bounds. Keyed by normalized mod id (+ digit-stripped form).

#include "TabletTypes.h"
#include "../third_party/json.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace TabletHelper {

struct ModRange {
    int min = 0;
    int max = 0;
    std::string unit;  // percent | flat | seconds | none
};

class TabletRanges {
public:
    std::string Load(const std::filesystem::path& pluginDir) {
        m_ranges.clear();
        try {
            const auto path = pluginDir / "config" / "tablet_mod_ranges.json";
            if (!std::filesystem::exists(path))
                return "tablet_mod_ranges.json not found — value filters have no bounds.";
            std::ifstream in(path);
            if (!in.is_open()) return "tablet_mod_ranges.json could not be opened.";

            nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
            if (j.is_discarded() || !j.is_object()) return "tablet_mod_ranges.json is corrupt.";
            if (!j.contains("mods") || !j["mods"].is_array())
                return "tablet_mod_ranges.json has no mods array.";

            for (const auto& e : j["mods"]) {
                if (!e.is_object() || !e.contains("id") || !e["id"].is_string()) continue;
                ModRange r;
                if (e.contains("min") && e["min"].is_number_integer()) r.min = e["min"].get<int>();
                if (e.contains("max") && e["max"].is_number_integer()) r.max = e["max"].get<int>();
                r.unit = e.value("unit", std::string());
                const std::string norm = NormalizeIdentifier(e["id"].get<std::string>());
                if (norm.empty()) continue;
                m_ranges[norm] = r;
                const std::string stripped = StripTrailingDigits(norm);
                if (!stripped.empty() && stripped != norm) m_ranges.emplace(stripped, r);
            }

            char buf[96];
            std::snprintf(buf, sizeof(buf), "Loaded %zu tablet mod ranges.", m_ranges.size());
            return buf;
        } catch (...) {
            return "Failed to load tablet_mod_ranges.json.";
        }
    }

    const ModRange* For(const std::string& normId) const {
        auto it = m_ranges.find(normId);
        return it == m_ranges.end() ? nullptr : &it->second;
    }

private:
    std::unordered_map<std::string, ModRange> m_ranges;
};

}  // namespace TabletHelper
