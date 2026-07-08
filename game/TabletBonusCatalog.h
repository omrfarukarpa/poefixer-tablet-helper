#pragma once

// Precursor Tablet bonus catalog.
// A bonus maps a stable internal mod Id (e.g. "TowerBreachBossChance") to a
// human label + category. Matching is by internal mod identity only (never by
// translated tooltip text), because several bonuses can share wording.
//
// Per-type list = that type's specific bonuses + the shared common bonuses,
// de-duplicated by Id (first wins, so a type may override a common label) and
// sorted by Category then Label. Global = the union across every type.

#include "TabletTypes.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace TabletHelper {

struct Bonus {
    std::string Id;
    std::string Label;
    std::string Category;
    std::string NormId;          // NormalizeIdentifier(Id)
    std::string NormIdStripped;  // StripTrailingDigits(NormId)
};

// A bonus is present on a tablet if its normalized Id (or the trailing-digit-
// stripped form, to collapse tier variants) is in the tablet's mod key set.
inline bool BonusMatches(const Bonus& b,
                         const std::unordered_set<std::string>& keys) {
    if (keys.find(b.NormId) != keys.end()) return true;
    if (!b.NormIdStripped.empty() && b.NormIdStripped != b.NormId
        && keys.find(b.NormIdStripped) != keys.end())
        return true;
    return false;
}

namespace detail {

inline constexpr const char* kCommon   = "Common";
inline constexpr const char* kMechanic = "Mechanic-specific";
inline constexpr const char* kUnique   = "Unique tablet";

inline Bonus B(const char* id, const char* label, const char* category) {
    Bonus b;
    b.Id = id;
    b.Label = label;
    b.Category = category;
    b.NormId = NormalizeIdentifier(id);
    b.NormIdStripped = StripTrailingDigits(b.NormId);
    return b;
}

inline const std::vector<Bonus>& CommonBonuses() {
    static const std::vector<Bonus> v = {
        B("TowerDroppedItemRarityIncrease", "Increased Rarity of Items found in Map", kCommon),
        B("TowerMapDroppedMapsIncrease", "Increased Quantity of Waystones found in Map", kCommon),
        B("TowerDroppedGoldIncrease", "Increased Gold found in Map", kCommon),
        B("TowerExperienceGainIncrease", "Increased Experience gain in Map", kCommon),
        B("TowerMonsterEffectiveness", "Monsters have increased Effectiveness", kCommon),
        B("TowerMonsterRarityIncrease", "Map has increased Monster Rarity", kCommon),
        B("TowerRarePackIncrease", "Map has increased number of Rare Monsters", kCommon),
        B("TowerMagicPackIncrease", "Map has increased Magic Monsters", kCommon),
        B("TowerPackSizeIncrease", "Increased Pack Size in Map", kCommon),
        B("TowerRareMonsterSurpassing", "Rare Monsters have Surpassing chance to have an additional Modifier", kCommon),
        B("TowerReducedPackSize", "Reduced Pack Size in Map", kCommon),
        B("TowerRareChestCount", "Map contains additional Rare Chests", kCommon),
        B("TowerAdditionalStoneCircle", "Map contains 1 additional Summoning Circle", kCommon),
        B("TowerAdditionalExile", "Map is inhabited by 1 additional Rogue Exile", kCommon),
        B("TowerAdditionalAzmeriWisp", "Map contains 1 additional Azmeri Spirit", kCommon),
        B("TowerAdditionalEssence", "Map contains 1 additional Essence", kCommon),
        B("TowerAdditionalShrine", "Map contains 1 additional Shrine", kCommon),
        B("TowerAdditionalStrongbox", "Map contains 1 additional Strongbox", kCommon),
        B("TowerStoneCircleChance", "Map has increased chance to contain a Summoning Circle", kCommon),
        B("TowerAdditionalExileChance", "Map has increased chance to contain Rogue Exiles", kCommon),
        B("TowerAdditionalSpiritChance", "Map has increased chance to contain Azmeri Spirits", kCommon),
        B("TowerAdditionalEssenceChance", "Map has increased chance to contain Essences", kCommon),
        B("TowerAdditionalShrineChance", "Map has increased chance to contain Shrines", kCommon),
        B("TowerAdditionalStrongboxChance", "Map has increased chance to contain Strongboxes", kCommon),
        B("TowerMapAdditionalModifier", "Map has additional random Modifiers", kCommon),
        B("TowerMapAdditionalUniqueMonsterModifier", "Unique Monsters have 1 additional Rare Modifier", kCommon),
    };
    return v;
}

inline std::vector<Bonus> SpecificBonuses(const std::string& typeKey) {
    if (typeKey == TypeKeys::Irradiated) {
        return {
            B("UniqueBiomeTabletForest", "Map also counts as a Forest Map", kUnique),
            B("UniqueBiomeTabletMountain", "Map also counts as a Mountain Map", kUnique),
            B("UniqueBiomeTabletWater", "Map also counts as a Water Map", kUnique),
        };
    }
    if (typeKey == TypeKeys::Breach) {
        return {
            B("TowerPackSizeIncrease", "Breaches in Map have increased Pack Size", kCommon),
            B("TowerBreachAdditionalRares", "Unstable Breaches spawn an additional Rare Monster when Stabilised", kMechanic),
            B("TowerBreachBossChance", "Unstable Breaches have increased chance to contain Vruun, Marshal of Xesht", kMechanic),
            B("TowerBreachWombgiftLevelChance", "Wombgifts have chance to drop one Level higher", kMechanic),
            B("TowerBreachWombgiftQuantity", "Increased Quantity of Wombgifts found in Map", kMechanic),
            B("TowerBreachHivebloodQuantity", "Increased Quantity of Hiveblood found in Map", kMechanic),
            B("TowerBreachRareMonsterPotency", "Increased Effectiveness of Rare Breach Monsters", kMechanic),
            B("TowerBreachMonsterQuantity", "Breaches have increased Monster density", kMechanic),
            B("UniqueBreachHiveAdditionalWaves", "Breach Hives have additional waves of Hiveborn Monsters", kUnique),
            B("UniqueBreachMinimumRadius", "Unstable Breaches take additional seconds to collapse after timer is filled", kUnique),
            B("UniqueBreachUnstableAdditionalRares", "Unstable Breaches spawn additional Rare Monsters when Stabilised", kUnique),
            B("UniqueTowerBreachDensityIncrease", "Breaches have changed Monster density", kUnique),
        };
    }
    if (typeKey == TypeKeys::Delirium) {
        return {
            B("TowerDeliriumAdditionalShardsChance", "Delirium Fog spawns increased Mirror Shards", kMechanic),
            B("TowerDeliriumRareMonsterPause", "Slaying Rare Monsters pauses the Delirium Mirror Timer", kMechanic),
            B("TowerDeliriumDoodadsIncrease", "Delirium Fog spawns increased Fracturing Mirrors", kMechanic),
            B("TowerDeliriumPackSizeIncrease", "Delirium Monsters have increased Pack Size", kMechanic),
            B("TowerDeliriumDifficultyIncrease", "Delirium Fog applies increased Deliriousness to Players", kMechanic),
            B("TowerDeliriumFogPersistence", "Delirium Fog dissipates slower", kMechanic),
            B("TowerDeliriumFogDissipationDelayNew", "Delirium Fog lasts additional seconds before dissipating", kMechanic),
            B("TowerDeliriumMonsterSplinterIncrease", "Increased Stack size of Simulacrum Splinters found in Map", kMechanic),
            B("TowerDeliriumBossChance", "Delirium Encounters are more likely to spawn Unique Bosses", kMechanic),
            B("UniqueDeliriumDifficultyIncrease", "Delirium Fog changes Deliriousness applied to Players", kUnique),
            B("UniqueDeliriumEndlessFog", "Delirium Fog in your Maps never dissipates", kUnique),
        };
    }
    if (typeKey == TypeKeys::Abyss) {
        return {
            B("TowerAbyssAdditionalChance", "Map contains an additional Abyss", kMechanic),
            B("TowerAbyss4AdditionalChance", "Map has chance to contain four additional Abysses", kMechanic),
            B("TowerAbyssExtraTickets", "Increased chance for Desecrated Currency from Abysses", kMechanic),
            B("TowerAbyssExtraModifiers", "Increased chance for Abyssal monsters to have Abyssal Modifiers", kMechanic),
            B("TowerAbyssIncreasedRewards", "Abyss Pits are twice as likely to have Rewards", kMechanic),
            B("TowerAbyssDepthsChance", "Abysses have increased chance to lead to an Abyssal Depths", kMechanic),
            B("TowerAbyssEffectivenessPerChasm", "Abyssal Monsters have increased Difficulty and Reward for each closed Pit", kMechanic),
            B("TowerAbyssRareMonsterIncrease", "Additional Rare Monsters are spawned from Abysses", kMechanic),
            B("TowerAbyssMonsterIncrease", "Abysses spawn increased Monsters", kMechanic),
        };
    }
    if (typeKey == TypeKeys::Ritual) {
        return {
            B("TowerRitualOmenChance", "Ritual Favours have increased chance to be Omens", kMechanic),
            B("TowerRitualMagicMonsters", "Revived Monsters from Ritual Altars have increased chance to be Rare", kMechanic),
            B("TowerRitualRareMonsters", "Revived Monsters from Ritual Altars have increased chance to be Magic", kMechanic),
            B("TowerRitualChanceForNoCost", "Favours Rerolled have chance to cost no Tribute", kMechanic),
            B("TowerRitualAdditionalReroll", "Ritual Altars allow rerolling Favours additional times", kMechanic),
            B("TowerRitualDeferSpeed", "Favours Deferred reappear sooner", kMechanic),
            B("TowerRitualDeferCostIncrease", "Deferring Favours costs reduced Tribute", kMechanic),
            B("TowerRitualRerollCostIncrease", "Rerolling Favours costs reduced Tribute", kMechanic),
            B("TowerRitualTributeIncrease", "Monsters Sacrificed at Ritual Altars grant increased Tribute", kMechanic),
            B("UniqueRitualTributeCostIncrease", "Favours at Ritual Altars cost increased Tribute", kUnique),
            B("UniqueRitualUnlimitedRerolls", "Can Reroll Favours at Ritual Altars twice as many times", kUnique),
        };
    }
    if (typeKey == TypeKeys::Overseer) {
        return {
            B("TowerMapBossExperience", "Map Bosses grant increased Experience", kMechanic),
            B("TowerMapBossWaystoneChance", "Increased Quantity of Waystones dropped by Map Bosses", kMechanic),
            B("TowerMapBossRarity", "Increased Rarity of Items dropped by Map Bosses", kMechanic),
            B("TowerMapBossQuantity", "Increased Quantity of Items dropped by Map Bosses", kMechanic),
            B("TowerMapBossAdditionalSpirit", "Areas with Powerful Map Bosses contain an additional Azmeri Spirit", kMechanic),
            B("TowerMapBossAdditionalEssence", "Areas with Powerful Map Bosses contain an additional Essence", kMechanic),
            B("TowerMapBossAdditionalShrine", "Areas with Powerful Map Bosses contain an additional Shrine", kMechanic),
            B("TowerMapBossAdditionalStrongbox", "Areas with Powerful Map Bosses contain additional Strongboxes", kMechanic),
            B("UniqueMapBossAdditionalModifier", "Map Bosses have an additional Modifier", kUnique),
            B("UniqueMapBossPossession", "Map Bosses are Hunted by Azmeri Spirits", kUnique),
        };
    }
    if (typeKey == TypeKeys::Temple) {
        return {
            B("TowerIncursionRareChestChance", "Increased chance Vaal Beacon Chests are Rare", kMechanic),
            B("TowerIncursionBossChance", "Chance to add a Vaal Beacon Unique Monster", kMechanic),
            B("TowerIncursionTokenChance", "Chance to gain an additional Crystal from Vaal Beacons", kMechanic),
            B("TowerIncursionSecondaryEncounters", "Increased chance Vaal Beacons summon additional Monsters", kMechanic),
            B("TowerIncursionExtraPacksChance", "Chance for extra packs of Monsters around Vaal Beacons", kMechanic),
            B("TowerIncursionExtraPacks", "1 extra pack of Monsters around Vaal Beacons", kMechanic),
            B("TowerIncursionPackSize", "Increased Pack Size for Monsters around Vaal Beacons", kMechanic),
        };
    }
    return {};
}

// De-duplicate by Id (case-insensitive, first wins) then sort by Category, Label.
inline std::vector<Bonus> DistinctSorted(std::vector<Bonus> in) {
    std::vector<Bonus> out;
    std::unordered_set<std::string> seen;
    for (auto& b : in) {
        std::string idLow = ToLowerCopy(b.Id);
        if (!seen.insert(idLow).second) continue;
        out.push_back(std::move(b));
    }
    std::sort(out.begin(), out.end(), [](const Bonus& a, const Bonus& b) {
        std::string ca = ToLowerCopy(a.Category), cb = ToLowerCopy(b.Category);
        if (ca != cb) return ca < cb;
        return ToLowerCopy(a.Label) < ToLowerCopy(b.Label);
    });
    return out;
}

inline const std::unordered_map<std::string, std::vector<Bonus>>& Catalog() {
    static const std::unordered_map<std::string, std::vector<Bonus>> cat = [] {
        std::unordered_map<std::string, std::vector<Bonus>> m;
        std::vector<Bonus> unionAll;
        for (const char* key : RealTypeKeys()) {
            std::vector<Bonus> list = SpecificBonuses(key);
            const auto& common = CommonBonuses();
            list.insert(list.end(), common.begin(), common.end());
            auto sorted = DistinctSorted(std::move(list));
            unionAll.insert(unionAll.end(), sorted.begin(), sorted.end());
            m.emplace(key, std::move(sorted));
        }
        m.emplace(TypeKeys::Global, DistinctSorted(std::move(unionAll)));
        return m;
    }();
    return cat;
}

}  // namespace detail

// Bonuses selectable for a given tablet type (or Global). Empty if the key is
// not a known tablet type.
inline const std::vector<Bonus>& GetBonusesFor(const std::string& typeKey) {
    static const std::vector<Bonus> kEmpty;
    const auto& cat = detail::Catalog();
    auto it = cat.find(typeKey);
    return it != cat.end() ? it->second : kEmpty;
}

inline bool IsKnownType(const std::string& key) {
    return detail::Catalog().find(key) != detail::Catalog().end();
}

// Look up a single bonus definition by Id within a type's (or Global's) list.
inline const Bonus* FindBonus(const std::string& typeKey, const std::string& id) {
    if (id.empty()) return nullptr;
    const std::string want = NormalizeIdentifier(id);  // matches precomputed b.NormId
    if (want.empty()) return nullptr;
    for (const auto& b : GetBonusesFor(typeKey))
        if (b.NormId == want) return &b;
    return nullptr;
}

}  // namespace TabletHelper
