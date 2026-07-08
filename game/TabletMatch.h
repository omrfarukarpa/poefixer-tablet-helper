#pragma once

// Match engine: given the settings and a parsed tablet, decide whether to
// highlight it, in which color, and what label lines to draw.
//
// A tablet is evaluated against TWO scopes: its own type's config and the Global
// config. Each scope accepts the tablet when it is enabled, the tablet has enough
// uses-left, and either the scope selects no bonuses (type-only highlight) or
// enough of its selected bonuses are present. Own-type color wins when both
// scopes accept. Unknown-type tablets are never highlighted.

#include "../config/Settings.h"
#include "TabletBonusCatalog.h"
#include "TabletScanner.h"
#include "TabletTypes.h"

#include <algorithm>
#include <string>
#include <vector>

namespace TabletHelper {

struct Evaluation {
    bool matched = false;
    float color[4] = {1.f, 1.f, 1.f, 1.f};  // copied from the winning TypeConfig
    std::vector<std::string> lines;          // composed label lines (may be empty)
};

inline bool IsRequired(const TabletHelperConfig::TypeConfig& c, const std::string& id) {
    return std::find(c.requiredBonusIds.begin(), c.requiredBonusIds.end(), id)
           != c.requiredBonusIds.end();
}

inline bool ConfigAccepts(const TabletHelperConfig::TypeConfig& c,
                          const ParsedTablet& t, const std::string& bonusScope,
                          std::vector<const Bonus*>* matchedOut = nullptr) {
    if (t.usesLeft < c.minUsesLeft) return false;
    if (c.selectedBonusIds.empty()) return true;  // no bonus filter -> match by type

    // Split the selected bonuses into a required and an optional pool, counting
    // how many of each are present on the tablet. Matching needs at least
    // minRequiredBonuses of the required pool AND minMatchedBonuses of the
    // optional pool; an empty pool imposes no condition. Required matches lead
    // the label list.
    std::vector<const Bonus*> reqMatched, optMatched;
    int reqTotal = 0, optTotal = 0;
    for (const auto& id : c.selectedBonusIds) {
        const bool req = IsRequired(c, id);
        (req ? reqTotal : optTotal) += 1;
        const Bonus* b = FindBonus(bonusScope, id);
        if (b && BonusMatches(*b, t.matchKeys))
            (req ? reqMatched : optMatched).push_back(b);
    }

    // Each threshold clamped to its pool size so a stale/hand-edited value can't
    // make matching permanently impossible.
    if (reqTotal > 0) {
        int need = c.minRequiredBonuses;
        if (need > reqTotal) need = reqTotal;
        if (need < 1) need = 1;
        if (static_cast<int>(reqMatched.size()) < need) return false;
    }
    if (optTotal > 0) {
        int need = c.minMatchedBonuses;
        if (need > optTotal) need = optTotal;
        if (need < 1) need = 1;
        if (static_cast<int>(optMatched.size()) < need) return false;
    }

    if (matchedOut) {
        reqMatched.insert(reqMatched.end(), optMatched.begin(), optMatched.end());
        *matchedOut = std::move(reqMatched);
    }
    return true;
}

inline Evaluation EvaluateTablet(const TabletHelperConfig::Settings& s,
                                 const ParsedTablet& t) {
    Evaluation e;
    if (!IsKnownType(t.typeKey)) return e;  // Unknown -> never highlighted

    const auto* own = s.FindType(t.typeKey);
    const auto* glob = s.FindType(TypeKeys::Global);

    std::vector<const Bonus*> ownMatched, globMatched;
    const bool ownOk = own && own->enabled && ConfigAccepts(*own, t, t.typeKey, &ownMatched);
    const bool globOk = glob && glob->enabled
                        && ConfigAccepts(*glob, t, TypeKeys::Global, &globMatched);
    if (!ownOk && !globOk) return e;

    e.matched = true;
    const float* src = ownOk ? own->color : glob->color;
    e.color[0] = src[0];
    e.color[1] = src[1];
    e.color[2] = src[2];
    e.color[3] = src[3];

    if (s.showLabel) {
        std::string primary = ShortName(t.typeKey);
        if (s.showUsesLeft) {
            primary += " (";
            primary += std::to_string(t.usesLeft);
            primary += ")";
        }
        e.lines.push_back(std::move(primary));

        if (s.showMatchedBonuses) {
            const auto& matched = ownOk ? ownMatched : globMatched;
            for (const auto* b : matched) {
                std::string lab = b->Label;
                if (lab.size() > 42) lab = lab.substr(0, 40) + "..";
                e.lines.push_back(std::move(lab));
            }
        }
    }
    return e;
}

}  // namespace TabletHelper
