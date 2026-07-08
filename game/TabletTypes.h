#pragma once

// Tablet type keys, detection and classification — ported from the ExileCore2
// TabletHelper (TabletItem.DetectTabletType / IsTabletEntity). Pure std, no host
// or ImGui dependency, so it can be unit-reasoned in isolation.

#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace TabletHelper {

// Stable ASCII type keys. These are the JSON keys and the catalog keys — they
// must never be localized (see the Turkish/encoding rule: identifiers stay ASCII).
namespace TypeKeys {
inline constexpr const char* Unknown    = "Unknown";
inline constexpr const char* Irradiated = "Irradiated";
inline constexpr const char* Breach     = "Breach";
inline constexpr const char* Delirium   = "Delirium";
inline constexpr const char* Abyss      = "Abyss";
inline constexpr const char* Ritual     = "Ritual";
inline constexpr const char* Overseer   = "Overseer";
inline constexpr const char* Temple     = "Temple";
inline constexpr const char* Global     = "Global";
}  // namespace TypeKeys

// The seven real tablet types, in the canonical order the original settings use.
// (Global is a cross-type scope handled separately.)
inline const std::array<const char*, 7>& RealTypeKeys() {
    static const std::array<const char*, 7> k = {
        TypeKeys::Irradiated, TypeKeys::Breach, TypeKeys::Delirium,
        TypeKeys::Abyss,      TypeKeys::Ritual, TypeKeys::Overseer,
        TypeKeys::Temple};
    return k;
}

inline std::string ToLowerCopy(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

// Allocation-free case-insensitive substring test. `needleLower` MUST already be
// lowercase. Used on the hot path (called for every item in every inventory).
inline bool ContainsCI(std::string_view hay, std::string_view needleLower) {
    if (needleLower.empty()) return true;
    if (needleLower.size() > hay.size()) return false;
    for (size_t i = 0; i + needleLower.size() <= hay.size(); ++i) {
        size_t j = 0;
        for (; j < needleLower.size(); ++j) {
            const char c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(hay[i + j])));
            if (c != needleLower[j]) break;
        }
        if (j == needleLower.size()) return true;
    }
    return false;
}

// Normalize a mod/bonus identifier to lowercase-alphanumeric only, matching
// TabletModInfo.NormalizeIdentifier. "TowerBreachBossChance1" -> "towerbreachbosschance1".
inline std::string NormalizeIdentifier(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value)
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

// Drop a trailing run of digits (tier suffixes): "towerbreachbosschance1" ->
// "towerbreachbosschance". Mirrors TabletModInfo.StripTrailingDigits.
inline std::string StripTrailingDigits(const std::string& v) {
    size_t end = v.size();
    while (end > 0 && std::isdigit(static_cast<unsigned char>(v[end - 1]))) --end;
    return v.substr(0, end);
}

// "Is this item a Precursor Tablet?" The original keys off the entity metadata
// path containing "TowerAugment". PoeFixer exposes that path as
// InventoryItem.Path (pre-materialized, crash-free). We also accept a base-type
// fallback so a tablet is still caught if Path is ever empty for an item.
inline bool LooksLikeTablet(const std::string& path, const std::string& baseType) {
    if (ContainsCI(path, "toweraugment")) return true;
    if (ContainsCI(baseType, "precursor tablet")) return true;
    return false;
}

// Classify into one of the seven types by substring cascade over the metadata
// path (plus base type name as a safety net). ORDER MATTERS — first match wins,
// exactly as TabletItem.DetectTabletType. Returns TypeKeys::Unknown if nothing
// matched (caller may then fall back to a mod-name scan).
inline const char* ClassifyFromText(const std::string& path,
                                    const std::string& baseType) {
    // A keyword present in EITHER the path or the base type name counts — same
    // effect as the original's combined-string cascade, without allocating.
    auto has = [&](const char* kw) {
        return ContainsCI(path, kw) || ContainsCI(baseType, kw);
    };
    if (has("abyss")) return TypeKeys::Abyss;
    if (has("breach")) return TypeKeys::Breach;
    if (has("delirium")) return TypeKeys::Delirium;
    if (has("ritual")) return TypeKeys::Ritual;
    if (has("boss") || has("overseer")) return TypeKeys::Overseer;
    if (has("incursion") || has("temple") || has("vaal")) return TypeKeys::Temple;
    if (has("generic") || has("irradiated") || has("toweraugment"))
        return TypeKeys::Irradiated;
    return TypeKeys::Unknown;
}

// Fallback classification from a mod-name blob (lowercased Name+Id+StatKey of the
// tablet's implicit mods). Mirrors the mod-text branch of DetectTabletType.
inline const char* ClassifyFromModText(const std::string& lowerModText) {
    if (lowerModText.empty()) return TypeKeys::Unknown;
    if (lowerModText.find("abyss") != std::string::npos) return TypeKeys::Abyss;
    if (lowerModText.find("breach") != std::string::npos) return TypeKeys::Breach;
    if (lowerModText.find("delirium") != std::string::npos) return TypeKeys::Delirium;
    if (lowerModText.find("ritual") != std::string::npos) return TypeKeys::Ritual;
    if (lowerModText.find("mapboss") != std::string::npos
        || lowerModText.find("boss") != std::string::npos)
        return TypeKeys::Overseer;
    if (lowerModText.find("incursion") != std::string::npos
        || lowerModText.find("beacon") != std::string::npos
        || lowerModText.find("vaal") != std::string::npos)
        return TypeKeys::Temple;
    if (lowerModText.find("irradiated") != std::string::npos) return TypeKeys::Irradiated;
    return TypeKeys::Unknown;
}

// Full display name, matching TabletItem.ToDisplayName.
inline const char* DisplayName(const std::string& key) {
    if (key == TypeKeys::Irradiated) return "Irradiated Tablet";
    if (key == TypeKeys::Breach)     return "Breach Tablet";
    if (key == TypeKeys::Delirium)   return "Delirium Tablet";
    if (key == TypeKeys::Abyss)      return "Abyss Tablet";
    if (key == TypeKeys::Ritual)     return "Ritual Tablet";
    if (key == TypeKeys::Overseer)   return "Overseer Tablet";
    if (key == TypeKeys::Temple)     return "Temple Tablet";
    if (key == TypeKeys::Global)     return "Global";
    return "Unknown Tablet";
}

// Compact name for the in-game overlay label (no trailing " Tablet").
inline const char* ShortName(const std::string& key) {
    if (key == TypeKeys::Irradiated) return "Irradiated";
    if (key == TypeKeys::Breach)     return "Breach";
    if (key == TypeKeys::Delirium)   return "Delirium";
    if (key == TypeKeys::Abyss)      return "Abyss";
    if (key == TypeKeys::Ritual)     return "Ritual";
    if (key == TypeKeys::Overseer)   return "Overseer";
    if (key == TypeKeys::Temple)     return "Temple";
    if (key == TypeKeys::Global)     return "Global";
    return "Unknown";
}

}  // namespace TabletHelper
