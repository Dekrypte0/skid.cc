#include "menu_ui.h"

#include <cstring>

namespace MenuUI {
namespace {

struct StyleStack {
    int colors = 0;
    int vars   = 0;
    void pushColor(ImGuiCol idx, ImVec4 c) { ImGui::PushStyleColor(idx, c); ++colors; }
    void pushVar(ImGuiStyleVar idx, float v) { ImGui::PushStyleVar(idx, v); ++vars; }
    void pushVar(ImGuiStyleVar idx, ImVec2 v) { ImGui::PushStyleVar(idx, v); ++vars; }
    void pop() {
        if (vars)   { ImGui::PopStyleVar(vars); vars = 0; }
        if (colors) { ImGui::PopStyleColor(colors); colors = 0; }
    }
};

StyleStack g_cardStack;
bool       g_cardOpen = false;
bool       g_cardHasItemWidth = false;
StyleStack g_stripStack;
bool       g_stripOpen = false;
StyleStack g_sideStack;
bool       g_sideOpen = false;
StyleStack g_contentStack;
bool       g_contentOpen = false;

ImDrawList* g_contentDl = nullptr;
struct PairTrack {
    int    frame = -1;
    bool   has   = false;
    ImVec2 min, max;
};
PairTrack g_pair;

struct Anim {
    float open = 0.f;
    int   tab  = 0;
};
Anim g_anim;

inline float Saturate(float x) { return (std::clamp)(x, 0.f, 1.f); }

static ImVec4 Darker(ImVec4 c, float amt) {
    return ImVec4(
        (std::max)(0.f, c.x - amt),
        (std::max)(0.f, c.y - amt),
        (std::max)(0.f, c.z - amt),
        c.w);
}

struct ShellPalette {
    ImVec4 bg, side, card, frame, border, text, muted, track;
};

ShellPalette Palette() {
    ShellPalette p;
    p.bg     = Config::menu_bg;
    p.side   = Config::menu_sidebar_bg;
    p.card   = Config::menu_child_bg;
    p.border = Config::menu_border;
    p.text   = Config::menu_text;
    p.muted  = Config::menu_text_muted;
    p.frame  = Darker(p.card, 0.03f);
    p.track  = Darker(p.bg, 0.03f);
    if (p.border.w < 0.06f) p.border.w = 0.10f;
    if (p.border.w > 0.20f) p.border.w = 0.16f;
    p.text.w  = 1.f;
    p.muted.w = 1.f;

    const float op = (std::clamp)(Config::menu_opacity, 0.55f, 1.f);
    p.bg.w = op;
    if (Config::menu_glass <= 0.25f) {
        // Flat skeet shell: solid surfaces, no glass wash.
        p.side.w  = 1.f;
        p.card.w  = 1.f;
        p.frame.w = 1.f;
        p.track.w = 1.f;
    } else {
        const float t = (op - 0.55f) / 0.45f;
        p.side.w  = 0.58f + 0.32f * t;
        p.card.w  = 0.38f + 0.40f * t;
        p.frame.w = 0.42f + 0.30f * t;
        p.track.w = p.frame.w;
    }
    return p;
}

static int Alpha8(float a) {
    return (int)(std::clamp)(a * 255.f, 0.f, 255.f);
}

void PaintGloss(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding, float strength = 1.f) {
    if (!dl) return;
    const float w = max.x - min.x;
    const float h = max.y - min.y;
    if (w < 6.f || h < 6.f) return;
    strength = Saturate(strength) * Saturate(Config::menu_glass);
    if (strength <= 0.01f) return;

    const float sheenH = (std::clamp)(h * 0.22f, 6.f, rounding + 4.f);
    dl->PushClipRect(min, max, true);
    dl->AddRectFilled(
        ImVec2(min.x, min.y),
        ImVec2(max.x, min.y + sheenH),
        IM_COL32(255, 255, 255, Alpha8(0.09f * strength)),
        rounding, ImDrawFlags_RoundCornersTop);
    dl->PopClipRect();
    dl->AddRect(min, max,
        IM_COL32(255, 255, 255, Alpha8(0.08f * strength)), rounding, 0, 1.f);
}

bool DrawValueSlider(const char* label, const char* id, float* v, float vmin, float vmax,
                     const char* fmt, bool asInt) {
    if (!v || !(vmax > vmin))
        return false;

    const float avail = ImGui::GetContentRegionAvail().x;
    const float trackH = 14.f;
    const float barH = 8.f;

    char val[48];
    if (asInt)
        std::snprintf(val, sizeof(val), fmt ? fmt : "%d", (int)*v);
    else
        std::snprintf(val, sizeof(val), fmt ? fmt : "%.3f", *v);

    const ImVec2 valSz = ImGui::CalcTextSize(val);

    ImGui::PushID(id ? id : label);

    const float x0 = ImGui::GetCursorPosX();
    ImGui::PushStyleColor(ImGuiCol_Text, WithA(TextMuted(), 0.95f));
    ImGui::TextUnformatted(label ? label : "");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.f, 0.f);
    ImGui::SetCursorPosX(x0 + avail - valSz.x);
    ImGui::PushStyleColor(ImGuiCol_Text, TextBright());
    ImGui::TextUnformatted(val);
    ImGui::PopStyleColor();

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##track", ImVec2((std::max)(8.f, avail), trackH));
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    bool changed = false;
    if (held) {
        const float t = Saturate((ImGui::GetIO().MousePos.x - p0.x) / (std::max)(1.f, avail));
        float nv = vmin + t * (vmax - vmin);
        if (asInt)
            nv = (float)(int)(nv + (nv >= 0.f ? 0.5f : -0.5f));
        nv = (std::clamp)(nv, vmin, vmax);
        if (nv != *v) {
            *v = nv;
            changed = true;
        }
    }

    const float frac = Saturate((*v - vmin) / (vmax - vmin));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float cy = p0.y + trackH * 0.5f;
    const ImVec2 b0(p0.x, cy - barH * 0.5f);
    const ImVec2 b1(p0.x + avail, cy + barH * 0.5f);
    const ImVec2 i0(b0.x + 1.f, b0.y + 1.f);
    const ImVec2 i1(b1.x - 1.f, b1.y - 1.f);

    dl->AddRectFilled(b0, b1, IM_COL32(0, 0, 0, 255));
    const int tt = hovered || held ? 0x3A : 0x34;
    const int tb = hovered || held ? 0x47 : 0x41;
    dl->AddRectFilledMultiColor(i0, i1,
        IM_COL32(tt, tt, tt, 255), IM_COL32(tt, tt, tt, 255),
        IM_COL32(tb, tb, tb, 255), IM_COL32(tb, tb, tb, 255));

    const float fillW = (i1.x - i0.x) * frac;
    if (fillW > 0.5f) {
        const ImVec4 ac = Config::menu_accent;
        const ImVec4 ad(ac.x * 0.78f, ac.y * 0.78f, ac.z * 0.78f, 1.f);
        dl->AddRectFilledMultiColor(i0, ImVec2(i0.x + fillW, i1.y),
            ToU32(ac), ToU32(ac), ToU32(ad), ToU32(ad));
    }

    ImGui::PopID();
    return changed;
}

} // namespace

void AnimTick(bool openTarget) {
    if (!openTarget) {
        g_anim.open = 0.f;
        return;
    }
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.f || dt > 0.05f)
        dt = 1.f / 60.f;
    g_anim.open = Saturate(g_anim.open + dt / 0.06f);
}

bool AnimVisible() { return g_anim.open > 0.001f; }
float OpenAlpha()  { return g_anim.open; }

void NotifyTab(int tab) {
    g_anim.tab = tab;
}

void ApplyTheme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* c = style.Colors;
    const ShellPalette p = Palette();
    const ImVec4 accent = Config::menu_accent;
    const float r = (std::clamp)(Config::menu_rounding, 0.f, 6.f);

    auto tint = [&](float a) { return ImVec4(accent.x, accent.y, accent.z, a); };

    c[ImGuiCol_WindowBg]             = p.bg;
    c[ImGuiCol_ChildBg]              = p.card;
    c[ImGuiCol_PopupBg]              = ImVec4(0.110f, 0.110f, 0.110f, 0.99f);
    c[ImGuiCol_Border]               = ImVec4(0.047f, 0.047f, 0.047f, 1.f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_Text]                 = p.text;
    c[ImGuiCol_TextDisabled]         = p.muted;

    c[ImGuiCol_FrameBg]              = ImVec4(0.059f, 0.059f, 0.059f, 1.f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.098f, 0.098f, 0.098f, 1.f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.122f, 0.122f, 0.122f, 1.f);

    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = c[ImGuiCol_TitleBgCollapsed] = p.bg;
    c[ImGuiCol_MenuBarBg]            = p.side;

    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0.18f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(1.f, 1.f, 1.f, 0.16f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.f, 1.f, 1.f, 0.28f);
    c[ImGuiCol_ScrollbarGrabActive]  = accent;

    c[ImGuiCol_CheckMark]            = accent;
    c[ImGuiCol_CheckboxSelectedBg]   = tint(0.85f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.302f, 0.302f, 0.302f, 1.f);
    c[ImGuiCol_SliderGrabActive]     = accent;

    c[ImGuiCol_Button]               = ImVec4(0.133f, 0.133f, 0.133f, 1.f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.157f, 0.157f, 0.157f, 1.f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.102f, 0.102f, 0.102f, 1.f);

    c[ImGuiCol_Header]               = ImVec4(0.102f, 0.102f, 0.102f, 1.f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.133f, 0.133f, 0.133f, 1.f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.157f, 0.157f, 0.157f, 1.f);

    c[ImGuiCol_Separator]            = ImVec4(0.157f, 0.157f, 0.157f, 1.f);
    c[ImGuiCol_SeparatorHovered]     = tint(0.40f);
    c[ImGuiCol_SeparatorActive]      = accent;

    c[ImGuiCol_ResizeGrip]           = ImVec4(1.f, 1.f, 1.f, 0.04f);
    c[ImGuiCol_ResizeGripHovered]    = tint(0.22f);
    c[ImGuiCol_ResizeGripActive]     = tint(0.36f);

    c[ImGuiCol_Tab]                  = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TabHovered]           = ImVec4(0.133f, 0.133f, 0.133f, 1.f);
    c[ImGuiCol_TabSelected]          = ImVec4(0.090f, 0.090f, 0.090f, 1.f);
    c[ImGuiCol_TabSelectedOverline]  = accent;
    c[ImGuiCol_TabDimmed]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TabDimmedSelected]    = ImVec4(0.078f, 0.078f, 0.078f, 1.f);

    c[ImGuiCol_TextSelectedBg]       = tint(0.28f);
    c[ImGuiCol_NavCursor]            = accent;
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.02f, 0.02f, 0.03f, 0.50f);

    c[ImGuiCol_TableHeaderBg]        = ImVec4(1.f, 1.f, 1.f, 0.04f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(1.f, 1.f, 1.f, 0.08f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(1.f, 1.f, 1.f, 0.04f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(1.f, 1.f, 1.f, 0.015f);

    style.WindowRounding    = r;
    style.ChildRounding     = r;
    style.FrameRounding     = 0.f;
    style.PopupRounding     = r;
    style.ScrollbarRounding = 0.f;
    style.GrabRounding      = 0.f;
    style.TabRounding       = 0.f;

    const bool compact = Config::menu_compact;
    const float dpi = Layout::DpiMul();
    style.WindowPadding     = ImVec2(0, 0);
    style.FramePadding      = compact ? ImVec2(6, 3) : ImVec2(8, 4);
    style.ItemSpacing       = compact ? ImVec2(6, 4) : ImVec2(8, 5);
    style.ItemInnerSpacing  = ImVec2(5, 3);
    style.CellPadding       = ImVec2(4, 3);
    style.IndentSpacing     = 10.0f;

    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;

    style.ScrollbarSize     = 5.0f;
    style.GrabMinSize       = 8.0f;
    style.WindowTitleAlign  = ImVec2(0.0f, 0.5f);
    style.ButtonTextAlign   = ImVec2(0.5f, 0.5f);

    if (dpi > 1.001f) {
        style.FramePadding.x     = floorf(style.FramePadding.x * dpi);
        style.FramePadding.y     = floorf(style.FramePadding.y * dpi);
        style.ItemSpacing.x      = floorf(style.ItemSpacing.x * dpi);
        style.ItemSpacing.y      = floorf(style.ItemSpacing.y * dpi);
        style.ItemInnerSpacing.x = floorf(style.ItemInnerSpacing.x * dpi);
        style.ItemInnerSpacing.y = floorf(style.ItemInnerSpacing.y * dpi);
        style.CellPadding.x      = floorf(style.CellPadding.x * dpi);
        style.CellPadding.y      = floorf(style.CellPadding.y * dpi);
        style.IndentSpacing      = floorf(style.IndentSpacing * dpi);
        style.ScrollbarSize      = floorf(style.ScrollbarSize * dpi);
        style.GrabMinSize        = floorf(style.GrabMinSize * dpi);
    }
    ImGui::GetIO().FontGlobalScale = 1.f;

    style.AntiAliasedLines  = true;
    style.AntiAliasedFill   = true;
    style.Alpha             = 1.0f;
    style.DisabledAlpha     = 0.40f;
}

void ApplyPreset(int idx) {
    Config::menu_preset         = idx;
    Config::menu_compact        = true;
    Config::menu_widgets_follow = true;

    switch (idx) {
    case 0: // Midnight OLED (Pure deep black + electric cyan)
        Config::menu_bg         = ImVec4(0.015f, 0.015f, 0.020f, 0.94f);
        Config::menu_child_bg   = ImVec4(0.055f, 0.060f, 0.075f, 0.70f);
        Config::menu_sidebar_bg = ImVec4(0.010f, 0.010f, 0.015f, 0.88f);
        Config::menu_border     = ImVec4(0.20f, 0.25f, 0.35f, 0.22f);
        Config::menu_text       = ImVec4(0.95f, 0.97f, 1.00f, 1.f);
        Config::menu_text_muted = ImVec4(0.48f, 0.52f, 0.60f, 1.f);
        Config::menu_accent     = ImVec4(0.20f, 0.80f, 1.00f, 1.f);
        Config::menu_rounding   = 6.0f;
        Config::menu_opacity    = 0.94f;
        Config::menu_glass      = 0.40f;
        break;
    case 1: // Steel Slate (Modern dark slate + ice sky blue)
    default:
        Config::menu_bg         = ImVec4(0.070f, 0.074f, 0.090f, 0.88f);
        Config::menu_child_bg   = ImVec4(0.130f, 0.138f, 0.160f, 0.60f);
        Config::menu_sidebar_bg = ImVec4(0.040f, 0.044f, 0.056f, 0.80f);
        Config::menu_border     = ImVec4(1.0f, 1.0f, 1.0f, 0.12f);
        Config::menu_text       = ImVec4(0.94f, 0.95f, 0.97f, 1.f);
        Config::menu_text_muted = ImVec4(0.56f, 0.58f, 0.64f, 1.f);
        Config::menu_accent     = ImVec4(0.42f, 0.68f, 0.92f, 1.f);
        Config::menu_rounding   = 6.0f;
        Config::menu_opacity    = 0.88f;
        Config::menu_glass      = 0.65f;
        break;
    case 2: // Nordic Frost (Arctic charcoal + frost ice blue)
        Config::menu_bg         = ImVec4(0.110f, 0.125f, 0.160f, 0.90f);
        Config::menu_child_bg   = ImVec4(0.160f, 0.185f, 0.230f, 0.65f);
        Config::menu_sidebar_bg = ImVec4(0.080f, 0.095f, 0.125f, 0.85f);
        Config::menu_border     = ImVec4(0.55f, 0.70f, 0.85f, 0.18f);
        Config::menu_text       = ImVec4(0.92f, 0.95f, 0.98f, 1.f);
        Config::menu_text_muted = ImVec4(0.55f, 0.62f, 0.70f, 1.f);
        Config::menu_accent     = ImVec4(0.53f, 0.75f, 0.92f, 1.f);
        Config::menu_rounding   = 7.0f;
        Config::menu_opacity    = 0.90f;
        Config::menu_glass      = 0.75f;
        break;
    case 3: // Cyberpunk Neon (Deep obsidian violet + electric magenta purple)
        Config::menu_bg         = ImVec4(0.055f, 0.040f, 0.075f, 0.92f);
        Config::menu_child_bg   = ImVec4(0.110f, 0.080f, 0.150f, 0.62f);
        Config::menu_sidebar_bg = ImVec4(0.035f, 0.025f, 0.050f, 0.86f);
        Config::menu_border     = ImVec4(0.75f, 0.40f, 0.95f, 0.22f);
        Config::menu_text       = ImVec4(0.96f, 0.93f, 0.98f, 1.f);
        Config::menu_text_muted = ImVec4(0.62f, 0.52f, 0.68f, 1.f);
        Config::menu_accent     = ImVec4(0.78f, 0.35f, 0.98f, 1.f);
        Config::menu_rounding   = 6.0f;
        Config::menu_opacity    = 0.92f;
        Config::menu_glass      = 0.70f;
        break;
    case 4: // Emerald Matrix (Deep dark moss + vivid emerald green)
        Config::menu_bg         = ImVec4(0.045f, 0.065f, 0.050f, 0.90f);
        Config::menu_child_bg   = ImVec4(0.090f, 0.130f, 0.100f, 0.60f);
        Config::menu_sidebar_bg = ImVec4(0.025f, 0.040f, 0.030f, 0.85f);
        Config::menu_border     = ImVec4(0.30f, 0.85f, 0.50f, 0.18f);
        Config::menu_text       = ImVec4(0.93f, 0.97f, 0.94f, 1.f);
        Config::menu_text_muted = ImVec4(0.50f, 0.64f, 0.54f, 1.f);
        Config::menu_accent     = ImVec4(0.24f, 0.88f, 0.54f, 1.f);
        Config::menu_rounding   = 6.0f;
        Config::menu_opacity    = 0.90f;
        Config::menu_glass      = 0.60f;
        break;
    case 5: // Sunset Crimson (Volcanic dark slate + warm coral crimson)
        Config::menu_bg         = ImVec4(0.075f, 0.045f, 0.045f, 0.90f);
        Config::menu_child_bg   = ImVec4(0.145f, 0.090f, 0.090f, 0.60f);
        Config::menu_sidebar_bg = ImVec4(0.050f, 0.028f, 0.028f, 0.85f);
        Config::menu_border     = ImVec4(0.95f, 0.40f, 0.35f, 0.18f);
        Config::menu_text       = ImVec4(0.98f, 0.94f, 0.94f, 1.f);
        Config::menu_text_muted = ImVec4(0.68f, 0.54f, 0.54f, 1.f);
        Config::menu_accent     = ImVec4(0.96f, 0.38f, 0.32f, 1.f);
        Config::menu_rounding   = 6.0f;
        Config::menu_opacity    = 0.90f;
        Config::menu_glass      = 0.65f;
        break;
    case 6: // Dracula Velvet (Dracula slate + iconic velvet rose pink)
        Config::menu_bg         = ImVec4(0.105f, 0.100f, 0.135f, 0.90f);
        Config::menu_child_bg   = ImVec4(0.160f, 0.150f, 0.205f, 0.65f);
        Config::menu_sidebar_bg = ImVec4(0.075f, 0.070f, 0.095f, 0.85f);
        Config::menu_border     = ImVec4(0.85f, 0.50f, 0.75f, 0.18f);
        Config::menu_text       = ImVec4(0.96f, 0.95f, 0.98f, 1.f);
        Config::menu_text_muted = ImVec4(0.62f, 0.58f, 0.68f, 1.f);
        Config::menu_accent     = ImVec4(0.96f, 0.45f, 0.72f, 1.f);
        Config::menu_rounding   = 6.0f;
        Config::menu_opacity    = 0.90f;
        Config::menu_glass      = 0.65f;
        break;
    case 7: // Solar Luxury Gold (Warm obsidian charcoal + luxury amber gold)
        Config::menu_bg         = ImVec4(0.065f, 0.060f, 0.050f, 0.90f);
        Config::menu_child_bg   = ImVec4(0.135f, 0.125f, 0.105f, 0.60f);
        Config::menu_sidebar_bg = ImVec4(0.040f, 0.035f, 0.030f, 0.85f);
        Config::menu_border     = ImVec4(0.90f, 0.75f, 0.30f, 0.18f);
        Config::menu_text       = ImVec4(0.98f, 0.96f, 0.92f, 1.f);
        Config::menu_text_muted = ImVec4(0.66f, 0.62f, 0.52f, 1.f);
        Config::menu_accent     = ImVec4(0.98f, 0.78f, 0.22f, 1.f);
        Config::menu_rounding   = 6.0f;
        Config::menu_opacity    = 0.90f;
        Config::menu_glass      = 0.65f;
        break;
    }
    ApplyTheme();
}

void DrawWindowFrame(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding) {
    if (!dl) return;
    const float w = max.x - min.x;
    const float h = max.y - min.y;
    if (w < 16.f || h < 16.f) return;

    // Skeet shell: 1px black outer edge, 1px graphite inner edge.
    dl->AddRect(min, max, IM_COL32(5, 5, 5, 255), rounding, 0, 1.f);
    dl->AddRect(ImVec2(min.x + 1.f, min.y + 1.f), ImVec2(max.x - 1.f, max.y - 1.f),
        IM_COL32(40, 40, 40, 255), (std::max)(0.f, rounding - 1.f), 0, 1.f);

    // Signature three-stop strip: cyan -> purple -> olive.
    const float y0 = min.y + 2.f;
    const float y1 = min.y + 4.f;
    const float mid = min.x + w * 0.5f;
    dl->AddRectFilledMultiColor(ImVec2(min.x + 2.f, y0), ImVec2(mid, y1),
        IM_COL32(0x60, 0x8B, 0x99, 255), IM_COL32(0x85, 0x4E, 0x87, 255),
        IM_COL32(0x85, 0x4E, 0x87, 255), IM_COL32(0x60, 0x8B, 0x99, 255));
    dl->AddRectFilledMultiColor(ImVec2(mid, y0), ImVec2(max.x - 2.f, y1),
        IM_COL32(0x85, 0x4E, 0x87, 255), IM_COL32(0xB1, 0xB5, 0x80, 255),
        IM_COL32(0xB1, 0xB5, 0x80, 255), IM_COL32(0x85, 0x4E, 0x87, 255));
}

void Section(const char* label) {
    if (!label) label = "";
    const bool compact = Config::menu_compact;
    ImGui::Dummy(ImVec2(0, compact ? 5.f : 7.f));
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleColor(ImGuiCol_Text, WithA(TextBright(), 0.92f));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    const ImVec2 ts = ImGui::CalcTextSize(label);
    const float y = p.y + ts.y * 0.5f;
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(p.x + ts.x + 8.f, y), ImVec2(p.x + w, y),
        IM_COL32(255, 255, 255, 18), 1.f);
    ImGui::Dummy(ImVec2(0, compact ? 2.f : 3.f));
}

void Gap(float mult) {
    ImGui::Dummy(ImVec2(0, ImGui::GetStyle().ItemSpacing.y * mult));
}

void SoftSeparator() {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(p.x, p.y), ImVec2(p.x + w, p.y),
        IM_COL32(255, 255, 255, 18), 1.f);
    ImGui::Dummy(ImVec2(0, Config::menu_compact ? 5.f : 6.f));
}

void BeginCard(const char* id, float width, bool autoY) {
    const Layout L = Layout::Current();
    const ShellPalette p = Palette();
    float w = width;
    if (w > 0.f) {
        const float avail = ImGui::GetContentRegionAvail().x;
        if (w > avail) w = (std::max)(1.f, avail);
    }

    g_cardStack = {};
    g_cardStack.pushColor(ImGuiCol_ChildBg, p.card);
    g_cardStack.pushColor(ImGuiCol_Border, Config::menu_border);
    g_cardStack.pushVar(ImGuiStyleVar_ChildRounding, L.childRound);
    g_cardStack.pushVar(ImGuiStyleVar_WindowPadding, ImVec2(L.cardPad, L.cardPad));
    g_cardStack.pushVar(ImGuiStyleVar_ItemSpacing,
        Config::menu_compact ? ImVec2(6.f, 4.f) : ImVec2(8.f, 5.f));

    ImGuiChildFlags flags = ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding;
    if (autoY) flags |= ImGuiChildFlags_AutoResizeY;

    ImGui::BeginChild(id, ImVec2(w, 0), flags, ImGuiWindowFlags_None);
    g_cardOpen = true;
    ImGui::PushItemWidth(-1.f);
    g_cardHasItemWidth = true;
}

void EndCard() {
    if (g_cardHasItemWidth) { ImGui::PopItemWidth(); g_cardHasItemWidth = false; }
    if (g_cardOpen) {
        ImVec2 min = ImGui::GetWindowPos();
        ImVec2 max = ImVec2(min.x + ImGui::GetWindowSize().x, min.y + ImGui::GetWindowSize().y);
        ImGui::EndChild();
        g_cardOpen = false;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (dl && (max.x - min.x) > 4.f && (max.y - min.y) > 4.f)
            PaintGloss(dl, min, max, Layout::Current().childRound, 0.45f);

        const int frm = ImGui::GetFrameCount();
        if (g_pair.frame != frm) { g_pair = PairTrack{}; g_pair.frame = frm; }
        if (!g_pair.has) {
            g_pair.has = true; g_pair.min = min; g_pair.max = max;
        } else if (std::fabsf(min.y - g_pair.min.y) < 3.f) {
            ImDrawList* pdl = g_contentDl ? g_contentDl : dl;
            const float hA = g_pair.max.y - g_pair.min.y;
            const float hB = max.y - min.y;
            ImVec2 eMin, eMax;
            if (hB > hA + 1.f) {
                eMin = ImVec2(g_pair.min.x, g_pair.max.y - 1.f);
                eMax = ImVec2(g_pair.max.x, g_pair.max.y + hB - hA);
            } else if (hA > hB + 1.f) {
                eMin = ImVec2(min.x, max.y - 1.f);
                eMax = ImVec2(max.x, max.y + hA - hB);
            } else {
                eMin = eMax = ImVec2(0, 0);
            }
            if (pdl && eMax.y > eMin.y + 1.f && eMax.x > eMin.x + 1.f) {
                const ShellPalette p = Palette();
                const float cr = Layout::Current().childRound;
                pdl->AddRectFilled(eMin, eMax, ToU32(p.card), cr, ImDrawFlags_RoundCornersBottom);
                const ImU32 bc = ToU32(Config::menu_border);
                pdl->AddLine(ImVec2(eMin.x, eMin.y), ImVec2(eMin.x, eMax.y), bc, 1.f);
                pdl->AddLine(ImVec2(eMax.x - 1.f, eMin.y), ImVec2(eMax.x - 1.f, eMax.y), bc, 1.f);
                pdl->AddLine(ImVec2(eMin.x, eMax.y - 1.f), ImVec2(eMax.x, eMax.y - 1.f), bc, 1.f);
            }
            g_pair.has = false;
        } else {
            g_pair.has = true; g_pair.min = min; g_pair.max = max;
        }
    }
    g_cardStack.pop();
}

void BeginStrip(const char* id) {
    const Layout L = Layout::Current();
    const ShellPalette p = Palette();
    g_stripStack = {};
    g_stripStack.pushColor(ImGuiCol_ChildBg, p.card);
    g_stripStack.pushColor(ImGuiCol_Border, Config::menu_border);
    g_stripStack.pushVar(ImGuiStyleVar_ChildRounding, L.childRound);
    g_stripStack.pushVar(ImGuiStyleVar_WindowPadding,
        ImVec2(L.compact ? 8.f : 10.f, L.compact ? 6.f : 8.f));
    ImGui::BeginChild(id, ImVec2(-1.f, 0.f),
        ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_None);
    g_stripOpen = true;
}

void EndStrip() {
    if (g_stripOpen) {
        ImVec2 min = ImGui::GetWindowPos();
        ImVec2 max = ImVec2(min.x + ImGui::GetWindowSize().x, min.y + ImGui::GetWindowSize().y);
        ImGui::EndChild();
        g_stripOpen = false;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (dl && (max.x - min.x) > 4.f && (max.y - min.y) > 4.f)
            PaintGloss(dl, min, max, Layout::Current().childRound, 0.45f);
    }
    g_stripStack.pop();
}

bool Slider(const char* label, const char* id, float* v, float vmin, float vmax, const char* fmt) {
    return DrawValueSlider(label, id, v, vmin, vmax, fmt, false);
}

bool SliderInt(const char* label, const char* id, int* v, int vmin, int vmax, const char* fmt) {
    if (!v)
        return false;
    float fv = (float)*v;
    const bool changed = DrawValueSlider(label, id, &fv, (float)vmin, (float)vmax, fmt, true);
    if (changed)
        *v = (int)fv;
    return changed;
}

bool Combo(const char* label, const char* id, int* cur, const char* const items[], int count) {
    ImGui::PushStyleColor(ImGuiCol_Text, WithA(TextMuted(), 0.95f));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1.f);
    return ImGui::Combo(id, cur, items, count);
}

void Tip(const char* text) {
    if (text && text[0] && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", text);
}

bool FeatureToggle(const char* label, bool* v, const char* tip) {
    if (!v)
        return false;
    const bool compact = Config::menu_compact;
    const float rowH = compact ? 20.f : 22.f;
    const float box  = compact ? 11.f : 12.f;
    const float avail = ImGui::GetContentRegionAvail().x;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();

    ImGui::PushID(label ? label : "ft");
    ImGui::InvisibleButton("##hit", ImVec2((std::max)(8.f, avail), rowH));
    const bool hovered = ImGui::IsItemHovered();
    const bool pressed = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    if (pressed)
        *v = !*v;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float midY = p0.y + rowH * 0.5f;
    const ImVec2 b0(p0.x, midY - box * 0.5f);
    const ImVec2 b1(p0.x + box, midY + box * 0.5f);
    const ImVec2 i0(b0.x + 1.f, b0.y + 1.f);
    const ImVec2 i1(b1.x - 1.f, b1.y - 1.f);

    dl->AddRectFilled(b0, b1, IM_COL32(0, 0, 0, 255));
    if (*v) {
        const ImVec4 ac = Config::menu_accent;
        const ImVec4 ad(ac.x * 0.78f, ac.y * 0.78f, ac.z * 0.78f, 1.f);
        dl->AddRectFilledMultiColor(i0, i1,
            ToU32(ac), ToU32(ac), ToU32(ad), ToU32(ad));
    } else {
        const int top = hovered ? 0x55 : 0x4D;
        const int bot = hovered ? 0x37 : 0x2F;
        dl->AddRectFilledMultiColor(i0, i1,
            IM_COL32(top, top, top, 255), IM_COL32(top, top, top, 255),
            IM_COL32(bot, bot, bot, 255), IM_COL32(bot, bot, bot, 255));
    }

    if (label && label[0]) {
        const ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(p0.x + box + 8.f, midY - ts.y * 0.5f),
            ToU32(TextBright()), label);
    }

    if (hovered) {
        if (tip && tip[0])
            ImGui::SetTooltip("%s\nRight-click for settings.", tip);
        else
            ImGui::SetTooltip("Right-click for settings.");
    }
    ImGui::PopID();
    return pressed;
}

void PopupTitle(const char* title) {
    ImGui::PushStyleColor(ImGuiCol_Text, TextBright());
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    SoftSeparator();
}

bool NavIcon(const char* label, const char* iconUtf8, bool selected, const ImVec2& size, int index) {
    (void)index;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));

    char idBuf[64];
    std::snprintf(idBuf, sizeof(idBuf), "##nav_%s", label ? label : "x");
    const bool pressed = ImGui::Button(idBuf, size);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 a(min.x + 2.f, min.y + 2.f);
    const ImVec2 b(max.x - 2.f, max.y - 2.f);
    const float rr = 0.f;

    if (hovered && !selected)
        dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, 12), rr);
    if (selected) {
        dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, 9), rr);
        dl->AddRectFilled(ImVec2(a.x, a.y + 6.f), ImVec2(a.x + 2.f, b.y - 6.f),
            AccentU32(1.f), 0.f);
    }

    const ImU32 iconCol = selected ? AccentU32(1.f)
        : (hovered ? ToU32(TextBright()) : ToU32(WithA(TextMuted(), 0.78f)));

    if (iconUtf8 && iconUtf8[0] && g_MenuIconFont) {
        const float iconSz = 16.f;
        const ImVec2 isz = g_MenuIconFont->CalcTextSizeA(iconSz, FLT_MAX, 0.f, iconUtf8);
        const float cx = floorf((min.x + max.x) * 0.5f - isz.x * 0.5f);
        float cy = floorf((min.y + max.y) * 0.5f - isz.y * 0.5f);
        if (Config::menu_sidebar_labels && label && label[0]) {
            ImFont* font = ImGui::GetFont();
            // Draw at native atlas size - downscaling to a fixed 12px blurs glyphs.
            // Cap at 18 so labels never collide with the icon row (navBtnH 48).
            const float ls = (std::min)(ImGui::GetFontSize(), 18.f);
            const ImVec2 lsz = font->CalcTextSizeA(ls, FLT_MAX, 0.f, label);
            const float lx = floorf((min.x + max.x) * 0.5f - lsz.x * 0.5f);
            const float ly = floorf(max.y - lsz.y - 4.f);
            dl->AddText(font, ls, ImVec2(lx, ly), iconCol, label);
            cy = floorf(min.y + (max.y - min.y) * 0.42f - isz.y * 0.5f);
        }
        dl->AddText(g_MenuIconFont, iconSz, ImVec2(cx, cy), iconCol, iconUtf8);
    }

    if (label && label[0] && !Config::menu_sidebar_labels
        && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", label);

    return pressed;
}

bool NavButton(const char* label, const char* iconUtf8, bool selected, const ImVec2& size, int index) {
    if (size.x > 0.f && size.x <= 80.f && iconUtf8 && iconUtf8[0])
        return NavIcon(label, iconUtf8, selected, size, index);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.05f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));

    char idBuf[64];
    std::snprintf(idBuf, sizeof(idBuf), "##nav_%s", label ? label : "x");
    const bool pressed = ImGui::Button(idBuf, size);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool hovered = ImGui::IsItemHovered();
    if (selected) {
        dl->AddRectFilled(ImVec2(min.x + 2.f, min.y + 2.f),
            ImVec2(max.x - 2.f, max.y - 2.f), IM_COL32(255, 255, 255, 9), 0.f);
        dl->AddRectFilled(ImVec2(min.x + 2.f, min.y + 6.f),
            ImVec2(min.x + 4.f, max.y - 6.f), AccentU32(1.f), 0.f);
    }
    const ImU32 col = selected || hovered ? ToU32(TextBright()) : ToU32(TextMuted());
    const ImU32 iconCol = selected ? AccentU32(1.f) : col;

    float x = min.x + 10.f;
    const float midY = (min.y + max.y) * 0.5f;
    if (iconUtf8 && iconUtf8[0] && g_MenuIconFont) {
        const float iconSz = 16.f;
        const ImVec2 isz = g_MenuIconFont->CalcTextSizeA(iconSz, FLT_MAX, 0.f, iconUtf8);
        dl->AddText(g_MenuIconFont, iconSz, ImVec2(x, midY - isz.y * 0.5f), iconCol, iconUtf8);
        x += isz.x + 8.f;
    }
    if (label && label[0]) {
        ImFont* font = ImGui::GetFont();
        const float fs = ImGui::GetFontSize();
        const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.f, label);
        dl->AddText(font, fs, ImVec2(x, midY - tsz.y * 0.5f), col, label);
    }
    return pressed;
}

bool NavButton(const char* label, bool selected, const ImVec2& size, int index) {
    return NavButton(label, nullptr, selected, size, index);
}

void SubNav(const char* const* labels, int count, int* selected) {
    if (!labels || count <= 0 || !selected) return;

    static const char* s_selKey = nullptr;
    static int s_selVal = -1;
    if (s_selKey != labels[0] || s_selVal != *selected) {
        s_selKey = labels[0];
        s_selVal = *selected;
        ImGui::SetScrollY(0.f);
    }

    const Layout L = Layout::Current();
    const float h = L.subNavH;
    const float avail = ImGui::GetContentRegionAvail().x;
    const ImVec2 trackMin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddLine(
        ImVec2(trackMin.x, trackMin.y + h - 0.5f),
        ImVec2(trackMin.x + avail, trackMin.y + h - 0.5f),
        IM_COL32(255, 255, 255, 16), 1.f);

    const float baseW = floorf(avail / (float)count);
    const float lastW = (std::max)(1.f, avail - baseW * (count - 1));

    ImGui::SetCursorScreenPos(trackMin);
    ImGui::BeginGroup();
    for (int i = 0; i < count; ++i) {
        if (i > 0) ImGui::SameLine(0, 0.f);
        const float w = (i == count - 1) ? lastW : baseW;
        const bool sel = (*selected == i);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.04f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.06f));
        ImGui::PushStyleColor(ImGuiCol_Text, sel ? TextBright() : TextMuted());

        char idBuf[64];
        std::snprintf(idBuf, sizeof(idBuf), "%s##sub_%d", labels[i], i);
        if (ImGui::Button(idBuf, ImVec2(w, h)))
            *selected = i;

        if (sel) {
            const ImVec2 bmin = ImGui::GetItemRectMin();
            const ImVec2 bmax = ImGui::GetItemRectMax();
            dl->AddRectFilled(
                ImVec2(bmin.x + 10.f, bmax.y - 2.f),
                ImVec2(bmax.x - 10.f, bmax.y),
                AccentU32(0.95f), 1.f);
        }

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
    }
    ImGui::EndGroup();
    ImGui::SetCursorScreenPos(ImVec2(trackMin.x, trackMin.y + h + (Config::menu_compact ? 5.f : 7.f)));
    ImGui::Dummy(ImVec2(0, 0));
}

void BeginSidebar(const Layout& L, float height) {
    const ShellPalette p = Palette();
    g_sideStack = {};
    g_sideStack.pushColor(ImGuiCol_ChildBg, p.side);
    g_sideStack.pushColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 1.f));
    g_sideStack.pushVar(ImGuiStyleVar_ChildRounding, L.childRound);
    g_sideStack.pushVar(ImGuiStyleVar_WindowPadding,
        L.compact ? ImVec2(4.f, 6.f) : ImVec2(6.f, 8.f));
    g_sideStack.pushVar(ImGuiStyleVar_ItemSpacing,
        ImVec2(0.f, L.compact ? 3.f : 4.f));

    ImGui::BeginChild("##sidebar", ImVec2(L.sidebar, height),
        ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar);
    g_sideOpen = true;
}

void EndSidebar() {
    if (g_sideOpen) { ImGui::EndChild(); g_sideOpen = false; }
    g_sideStack.pop();
}

void BeginContent(const Layout& L, float contentW, float contentH) {
    const ShellPalette p = Palette();
    g_contentStack = {};
    g_contentStack.pushVar(ImGuiStyleVar_WindowPadding, ImVec2(L.contentPad, L.contentPad));
    g_contentStack.pushVar(ImGuiStyleVar_ChildRounding, L.childRound);
    g_contentStack.pushColor(ImGuiCol_ChildBg, WithA(p.bg, 0.0f));
    g_contentStack.pushColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));

    ImGui::BeginChild("##content", ImVec2(contentW, contentH),
        ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_None);
    g_contentOpen = true;
    g_contentDl = ImGui::GetWindowDrawList();
}

void EndContent() {
    if (g_contentOpen) { ImGui::EndChild(); g_contentOpen = false; }
    g_contentStack.pop();
}

namespace {
bool s_headerDrag = false;
}

void BeginHeader(const Layout& L, float windowW) {
    const ImVec2 wp = ImGui::GetWindowPos();
    const float h = L.headerH;
    ImGui::SetCursorPos(ImVec2(0.f, 0.f));
    ImGui::InvisibleButton("##win_drag", ImVec2(windowW, h));
    const bool hover = ImGui::IsItemHovered();
    const bool held  = ImGui::IsItemActive();

    if (hover && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        const ImGuiIO& io = ImGui::GetIO();
        const ImVec2 ws = ImGui::GetWindowSize();
        ImGui::SetWindowPos(ImVec2((io.DisplaySize.x - ws.x) * 0.5f, (io.DisplaySize.y - ws.y) * 0.5f));
        s_headerDrag = false;
    } else if (hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        s_headerDrag = true;
    }
    if (s_headerDrag && held) {
        const ImGuiIO& io = ImGui::GetIO();
        const ImVec2 w0 = ImGui::GetWindowPos();
        ImGui::SetWindowPos(ImVec2(w0.x + io.MouseDelta.x, w0.y + io.MouseDelta.y));
    } else if (!held) {
        s_headerDrag = false;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine(
        ImVec2(wp.x + L.shellPad, wp.y + h),
        ImVec2(wp.x + windowW - L.shellPad, wp.y + h),
        IM_COL32(40, 40, 40, 255), 1.f);

    ImGui::SetCursorPos(ImVec2(L.contentPad, (h - ImGui::GetFontSize()) * 0.5f));
}

void HeaderBrand(const char* name, const char* accentSuffix, const char* badge) {
    if (name && name[0]) {
        ImGui::PushStyleColor(ImGuiCol_Text, TextBright());
        ImGui::TextUnformatted(name);
        ImGui::PopStyleColor();
    }
    if (accentSuffix && accentSuffix[0]) {
        ImGui::SameLine(0, 6.f);
        ImGui::PushStyleColor(ImGuiCol_Text, Accent());
        ImGui::TextUnformatted(accentSuffix);
        ImGui::PopStyleColor();
    }
    if (badge && badge[0]) {
        ImGui::SameLine(0, 8.f);
        ImGui::PushStyleColor(ImGuiCol_Text, TextMuted());
        ImGui::TextUnformatted(badge);
        ImGui::PopStyleColor();
    }
}

void HeaderRightHint(const char* text) {
    if (!text || !text[0]) return;
    const Layout L = Layout::Current();
    const ImVec2 ws = ImGui::GetWindowSize();
    const ImVec2 ts = ImGui::CalcTextSize(text);
    ImGui::SetCursorPos(ImVec2(ws.x - ts.x - L.contentPad, (L.headerH - ts.y) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, WithA(TextMuted(), 0.85f));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

} // namespace MenuUI
