#pragma once

// On-screen geometry helpers — a hybrid rect resolver. PoeFixer enumerates EVERY
// inventory (backpack, equipment, cursor, and every stash tab you own, 100+). An
// item is only laid out where you can see it in the *visible* tab, so visibility
// is derived per item/grid rather than by identifying inventory ids.
//
// Rect resolution is a hybrid and ORDER MATTERS: per-item screen rect first
// (special/affinity tabs — currency/fragment/tablet-stash — carry correct
// per-item rects but a logical grid that doesn't match their visual layout),
// then grid math as a fallback (normal grid tabs report ScreenValid=0 and fall
// through to GridScreen + slot*CellSize). This is exactly why the special
// Tablet stash tab needs no UI-tree hack on PoeFixer.

#include "sdk/PluginSDK.h"

#include <optional>

namespace TabletHelper {

struct ScreenRect {
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;
};

// True if the grid's on-screen rectangle sits inside the game window. Closed
// stash tabs are enumerated but not laid out on screen.
inline bool GridOnScreen(const PluginSDK::Inventory& inv, float displayW, float displayH) {
    if (!inv.Grid.Valid || inv.Grid.CellSize <= 0.f) return false;
    if (displayW <= 0.f || displayH <= 0.f) return false;
    const float x = inv.Grid.GridScreenX;
    const float y = inv.Grid.GridScreenY;
    const float margin = 4.f;
    return x >= -margin && y >= -margin && x < displayW && y < displayH;
}

// True if a single item is actually rendered on screen. PoeFixer only assigns a
// real screen rect to items in the visible tab; hidden affinity tabs report
// ScreenValid=false or an off-screen rect.
inline bool ItemOnScreen(const PluginSDK::InventoryItem& item, float displayW, float displayH) {
    if (!item.ScreenValid) return false;
    if (item.ScreenW <= 0.f || item.ScreenH <= 0.f) return false;
    if (displayW <= 0.f || displayH <= 0.f) return false;
    const float cx = item.ScreenX + item.ScreenW * 0.5f;
    const float cy = item.ScreenY + item.ScreenH * 0.5f;
    return cx >= 0.f && cy >= 0.f && cx < displayW && cy < displayH;
}

// Reject absurd grid layouts before trusting grid math (a special tab's logical
// grid can be e.g. 53x4 and overflow the screen).
inline bool GridLayoutPlausible(const PluginSDK::Inventory& inv, float displayW) {
    if (inv.TotalBoxesY < 6) return false;
    if (inv.Grid.CellSize > 0.f
        && static_cast<float>(inv.TotalBoxesX) * inv.Grid.CellSize > displayW)
        return false;
    return true;
}

inline std::optional<ScreenRect> ResolveItemRect(const PluginSDK::Inventory& inv,
                                                 const PluginSDK::InventoryItem& item,
                                                 float displayW, float displayH) {
    if (ItemOnScreen(item, displayW, displayH)) {
        return ScreenRect{item.ScreenX, item.ScreenY, item.ScreenW, item.ScreenH};
    }
    if (inv.Grid.Valid && GridOnScreen(inv, displayW, displayH)
        && inv.Grid.CellSize > 0.f && GridLayoutPlausible(inv, displayW)) {
        const float cell = inv.Grid.CellSize;
        return ScreenRect{
            inv.Grid.GridScreenX + static_cast<float>(item.SlotX) * cell,
            inv.Grid.GridScreenY + static_cast<float>(item.SlotY) * cell,
            static_cast<float>(item.Width) * cell,
            static_cast<float>(item.Height) * cell};
    }
    return std::nullopt;
}

}  // namespace TabletHelper
