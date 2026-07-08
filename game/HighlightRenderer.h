#pragma once

// Highlight rendering — Border / Cross / Star plus labels, drawn on the ImGui
// foreground draw list. PoeFixer's item screen rects are already in ImGui screen
// space (the overlay is full-screen over the game), so rects map to draw
// coordinates with no transform.
//
// NOMINMAX is NOT defined for this build, so <Windows.h>'s min/max macros could
// be live; this file avoids std::min/std::max and uses fminf/fmaxf / ternaries.

#include "../config/Settings.h"
#include "PanelDetector.h"
#include "TabletMatch.h"

#include <imgui.h>

#include <cmath>
#include <string>
#include <vector>

namespace TabletHelper {

inline constexpr float kPi = 3.14159265358979323846f;

// Below this cell size (quad-stash / heavily downscaled), labels don't fit — the
// original suppresses labels in quad tabs; this is the host-agnostic equivalent.
inline constexpr float kMinLabelCellW = 26.f;
inline constexpr float kMinLabelCellH = 20.f;

inline ImU32 ColF(const float c[4]) {
    return IM_COL32(static_cast<int>(c[0] * 255.f + 0.5f),
                    static_cast<int>(c[1] * 255.f + 0.5f),
                    static_cast<int>(c[2] * 255.f + 0.5f),
                    static_cast<int>(c[3] * 255.f + 0.5f));
}

inline ImU32 ColFOpaque(const float c[4]) {
    return IM_COL32(static_cast<int>(c[0] * 255.f + 0.5f),
                    static_cast<int>(c[1] * 255.f + 0.5f),
                    static_cast<int>(c[2] * 255.f + 0.5f), 255);
}

inline void DrawBorderHighlight(ImDrawList* dl, const ScreenRect& r, ImU32 col, int thickness) {
    const int scale = thickness - 1;
    const float ix = static_cast<float>(static_cast<int>(r.x) + 1 + static_cast<int>(0.5f * scale));
    const float iy = static_cast<float>(static_cast<int>(r.y) + 1 + static_cast<int>(0.5f * scale));
    const float iw = static_cast<float>(static_cast<int>(r.w) - 1 - scale);
    const float ih = static_cast<float>(static_cast<int>(r.h) - 1 - scale);
    dl->AddRect(ImVec2(ix, iy), ImVec2(ix + iw, iy + ih), col, 0.f, 0, static_cast<float>(thickness));
}

inline void DrawCrossHighlight(ImDrawList* dl, const ScreenRect& r, ImU32 col, int thickness) {
    const ImU32 shadow = IM_COL32(0, 0, 0, 180);
    const float inset = thickness > 2 ? static_cast<float>(thickness) : 2.f;
    const ImVec2 tl(r.x + inset, r.y + inset);
    const ImVec2 tr(r.x + r.w - inset, r.y + inset);
    const ImVec2 bl(r.x + inset, r.y + r.h - inset);
    const ImVec2 br(r.x + r.w - inset, r.y + r.h - inset);
    const ImVec2 so(1.f, 1.f);
    dl->AddLine(ImVec2(tl.x + so.x, tl.y + so.y), ImVec2(br.x + so.x, br.y + so.y), shadow, thickness + 2.f);
    dl->AddLine(ImVec2(tr.x + so.x, tr.y + so.y), ImVec2(bl.x + so.x, bl.y + so.y), shadow, thickness + 2.f);
    dl->AddLine(tl, br, col, static_cast<float>(thickness));
    dl->AddLine(tr, bl, col, static_cast<float>(thickness));
}

inline ImVec2 StarPoint(ImVec2 center, float outer, float inner, int index) {
    const float angle = -kPi / 2.f + index * kPi / 5.f;
    const float radius = (index % 2 == 0) ? outer : inner;
    return ImVec2(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
}

inline void StarLines(ImDrawList* dl, ImVec2 center, float outer, float inner, ImU32 col, float thickness) {
    ImVec2 prev = StarPoint(center, outer, inner, 9);
    for (int i = 0; i < 10; ++i) {
        ImVec2 next = StarPoint(center, outer, inner, i);
        dl->AddLine(prev, next, col, thickness);
        prev = next;
    }
}

inline void DrawStarHighlight(ImDrawList* dl, const ScreenRect& r, ImU32 col, int thickness,
                              float labelLineHeight, int labelLines) {
    const float labelOffset = labelLines > 0 ? labelLines * labelLineHeight + 4.f : 0.f;
    const float minSide = (r.w < r.h) ? r.w : r.h;
    float outer = minSide * 0.18f;
    outer = outer < 7.f ? 7.f : (outer > 13.f ? 13.f : outer);
    const float inner = outer * 0.45f;
    const float centerX = r.x + outer + 5.f;
    const float preferredY = r.y + outer + 5.f + labelOffset;
    const float maxY = r.y + r.h - outer - 4.f;
    const ImVec2 center(centerX, preferredY < maxY ? preferredY : maxY);
    const ImU32 shadow = IM_COL32(0, 0, 0, 190);
    const float st = thickness > 2 ? static_cast<float>(thickness) : 2.f;
    StarLines(dl, ImVec2(center.x + 1.f, center.y + 1.f), outer, inner, shadow, st + 2.f);
    StarLines(dl, center, outer, inner, col, st);
}

inline float LabelFontSize(const TabletHelperConfig::Settings& s) {
    float scale = s.labelScale;
    scale = scale < TabletHelperConfig::kLabelScaleMin ? TabletHelperConfig::kLabelScaleMin
          : (scale > TabletHelperConfig::kLabelScaleMax ? TabletHelperConfig::kLabelScaleMax : scale);
    const float fs = ImGui::GetFontSize() * scale;
    return fs > 8.f ? fs : 8.f;
}

inline float LabelLineHeight(const TabletHelperConfig::Settings& s) {
    const float lh = LabelFontSize(s) + 2.f;
    return lh > 10.f ? lh : 10.f;
}

inline void DrawOutlinedText(ImDrawList* dl, ImFont* font, float fontSize, ImVec2 pos,
                             ImU32 col, const char* text) {
    const ImU32 outline = IM_COL32(0, 0, 0, 230);
    const float o = fmaxf(1.f, fontSize / 16.f);
    dl->AddText(font, fontSize, ImVec2(pos.x - o, pos.y), outline, text);
    dl->AddText(font, fontSize, ImVec2(pos.x + o, pos.y), outline, text);
    dl->AddText(font, fontSize, ImVec2(pos.x, pos.y - o), outline, text);
    dl->AddText(font, fontSize, ImVec2(pos.x, pos.y + o), outline, text);
    dl->AddText(font, fontSize, ImVec2(pos.x - o, pos.y - o), outline, text);
    dl->AddText(font, fontSize, ImVec2(pos.x + o, pos.y + o), outline, text);
    dl->AddText(font, fontSize, pos, col, text);
}

inline void DrawLabels(ImDrawList* dl, const ScreenRect& r,
                       const std::vector<std::string>& lines,
                       const TabletHelperConfig::Settings& s, const float color[4]) {
    ImFont* font = ImGui::GetFont();
    const float fontSize = LabelFontSize(s);
    const float lineHeight = LabelLineHeight(s);
    const float baseFontSize = fmaxf(1.f, ImGui::GetFontSize());
    const float textScale = fontSize / baseFontSize;
    const float x = r.x + 3.f;
    const float y = r.y + 2.f;

    const ImU32 textCol = (s.labelTextMode == TabletHelperConfig::LabelTextMode::TypeColor)
                              ? ColFOpaque(color)
                              : IM_COL32_WHITE;

    for (size_t i = 0; i < lines.size(); ++i) {
        const ImVec2 pos(x, y + static_cast<float>(i) * lineHeight);
        const char* text = lines[i].c_str();

        if (s.labelBackground) {
            const ImVec2 base = ImGui::CalcTextSize(text);
            const ImVec2 sz(base.x * textScale, base.y * textScale);
            const float px = 3.f, py = 1.f;
            const int a = s.labelBackgroundAlpha < 0 ? 0
                        : (s.labelBackgroundAlpha > 255 ? 255 : s.labelBackgroundAlpha);
            dl->AddRectFilled(ImVec2(pos.x - px, pos.y - py),
                              ImVec2(pos.x + sz.x + px, pos.y + sz.y + py),
                              IM_COL32(0, 0, 0, a), 2.f);
        }

        if (s.labelOutline)
            DrawOutlinedText(dl, font, fontSize, pos, textCol, text);
        else
            dl->AddText(font, fontSize, pos, textCol, text);
    }
}

// Draw one matched tablet: highlight (per style) plus labels, unless the cell is
// too small for labels (quad-stash equivalent).
inline void RenderTablet(const ScreenRect& r, const Evaluation& e,
                         const TabletHelperConfig::Settings& s) {
    if (!e.matched) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    int thickness = s.borderThickness;
    if (thickness < TabletHelperConfig::kBorderThicknessMin) thickness = TabletHelperConfig::kBorderThicknessMin;
    if (thickness > TabletHelperConfig::kBorderThicknessMax) thickness = TabletHelperConfig::kBorderThicknessMax;

    const ImU32 col = ColF(e.color);
    const bool roomForLabels =
        s.showLabel && !e.lines.empty() && r.w >= kMinLabelCellW && r.h >= kMinLabelCellH;
    const int labelLines = roomForLabels ? static_cast<int>(e.lines.size()) : 0;

    switch (s.highlightMode) {
        case TabletHelperConfig::HighlightMode::Cross:
            DrawCrossHighlight(dl, r, col, thickness);
            break;
        case TabletHelperConfig::HighlightMode::Star:
            DrawStarHighlight(dl, r, col, thickness, LabelLineHeight(s), labelLines);
            break;
        default:
            DrawBorderHighlight(dl, r, col, thickness);
            break;
    }

    if (roomForLabels) DrawLabels(dl, r, e.lines, s, e.color);
}

}  // namespace TabletHelper
