#include "theme.h"

#include "imgui_internal.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace theme {

float S = 1.0f;
ImVec4 Accent = ImVec4(149.0f / 255.0f, 212.0f / 255.0f, 84.0f / 255.0f, 1.0f);

ImFont* FontMain = nullptr;
ImFont* FontBold = nullptr;
ImFont* FontSmall = nullptr;
ImFont* FontSmallBold = nullptr;
ImFont* FontTitle = nullptr;

float SizeMain = 13.0f;
float SizeBold = 13.0f;
float SizeSmall = 10.0f;
float SizeSmallBold = 10.0f;
float SizeTitle = 18.0f;

namespace {

ImVector<ImWchar> g_ranges;

void BuildRanges()
{
    if (g_ranges.Size > 0)
        return;

    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesDefault());

    // Turkish + a few symbols used by the menu.
    static const ImWchar extra[] = {
        0x011E, 0x011F, 0x0130, 0x0131, 0x015E, 0x015F,
        0x00D7, 0x00B7, 0x2014, 0x2022, 0x25B6, 0x2713, 0x2026, 0x2192, 0x2190,
        0
    };
    builder.AddRanges(extra);
    builder.BuildRanges(&g_ranges);
}

ImFont* LoadFont(const char* const* candidates, float sizePx)
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig fc;
    fc.OversampleH = 2;
    fc.OversampleV = 1;
    fc.PixelSnapH = true;

    for (const char* const* p = candidates; *p; ++p)
    {
        if (GetFileAttributesA(*p) == INVALID_FILE_ATTRIBUTES)
            continue;
        if (ImFont* f = io.Fonts->AddFontFromFileTTF(*p, sizePx, &fc, g_ranges.Data))
            return f;
    }
    return io.Fonts->AddFontDefault();
}

} // namespace

ImU32 Hex(unsigned rgb, int a)
{
    return IM_COL32((int)((rgb >> 16) & 0xFF), (int)((rgb >> 8) & 0xFF), (int)(rgb & 0xFF), a);
}

ImU32 Acc(float k, int a)
{
    const int r = (int)ImClamp(Accent.x * 255.0f * k, 0.0f, 255.0f);
    const int g = (int)ImClamp(Accent.y * 255.0f * k, 0.0f, 255.0f);
    const int b = (int)ImClamp(Accent.z * 255.0f * k, 0.0f, 255.0f);
    return IM_COL32(r, g, b, a);
}

ImVec4 AccV(float k, float a)
{
    return ImVec4(ImClamp(Accent.x * k, 0.0f, 1.0f), ImClamp(Accent.y * k, 0.0f, 1.0f),
                  ImClamp(Accent.z * k, 0.0f, 1.0f), a);
}

void SetAccent(const ImVec4& accent)
{
    Accent = ImVec4(ImClamp(accent.x, 0.0f, 1.0f), ImClamp(accent.y, 0.0f, 1.0f),
                    ImClamp(accent.z, 0.0f, 1.0f), 1.0f);
    ApplyStyle();
}

void Init(float scale)
{
    S = (scale > 0.1f && scale < 4.0f) ? scale : 1.0f;

    SizeMain = std::round(13.0f * S);
    SizeBold = SizeMain;
    SizeSmall = std::round(10.0f * S);
    SizeSmallBold = SizeSmall;
    SizeTitle = std::round(18.0f * S);

    BuildRanges();

    static const char* const kRegular[] = {
        "C:\\Windows\\Fonts\\verdana.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
        nullptr
    };
    static const char* const kBold[] = {
        "C:\\Windows\\Fonts\\verdanab.ttf",
        "C:\\Windows\\Fonts\\segoeuib.ttf",
        nullptr
    };

    FontMain = LoadFont(kRegular, SizeMain);
    FontBold = LoadFont(kBold, SizeBold);
    FontSmall = LoadFont(kRegular, SizeSmall);
    FontSmallBold = LoadFont(kBold, SizeSmallBold);
    FontTitle = LoadFont(kBold, SizeTitle);

    ApplyStyle();
}

void ApplyStyle()
{
    ImGuiStyle& st = ImGui::GetStyle();

    st.WindowPadding = ImVec2(0.0f, 0.0f);
    st.FramePadding = ImVec2(8.0f * S, 4.0f * S);
    st.CellPadding = ImVec2(4.0f * S, 2.0f * S);
    st.ItemSpacing = ImVec2(8.0f * S, 4.0f * S);
    st.ItemInnerSpacing = ImVec2(6.0f * S, 4.0f * S);
    st.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    st.IndentSpacing = 18.0f * S;
    st.ScrollbarSize = 8.0f * S;
    st.GrabMinSize = 8.0f * S;

    st.WindowBorderSize = 0.0f;
    st.ChildBorderSize = 1.0f * S;
    st.PopupBorderSize = 1.0f * S;
    st.FrameBorderSize = 1.0f * S;
    st.TabBorderSize = 0.0f;

    st.WindowRounding = 4.0f * S;
    st.ChildRounding = 3.0f * S;
    st.PopupRounding = 3.0f * S;
    st.FrameRounding = 2.0f * S;
    st.ScrollbarRounding = 3.0f * S;
    st.GrabRounding = 2.0f * S;
    st.TabRounding = 2.0f * S;

    st.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    st.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    st.SelectableTextAlign = ImVec2(0.0f, 0.5f);

    ImVec4* c = st.Colors;
    c[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.43f, 0.43f, 0.43f, 1.00f);
    c[ImGuiCol_WindowBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    c[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    c[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    c[ImGuiCol_Border] = ImVec4(0.157f, 0.157f, 0.157f, 1.00f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg] = ImVec4(0.102f, 0.102f, 0.102f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.137f, 0.137f, 0.137f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.165f, 0.165f, 0.165f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.043f, 0.043f, 0.043f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.055f, 0.055f, 0.055f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.043f, 0.043f, 0.043f, 1.00f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.04f, 0.04f, 0.04f, 0.00f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive] = Accent;
    c[ImGuiCol_CheckMark] = Accent;
    c[ImGuiCol_SliderGrab] = Accent;
    c[ImGuiCol_SliderGrabActive] = AccV(1.15f);
    c[ImGuiCol_Button] = ImVec4(0.118f, 0.118f, 0.118f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.153f, 0.153f, 0.153f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.188f, 0.188f, 0.188f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    c[ImGuiCol_Separator] = ImVec4(0.157f, 0.157f, 0.157f, 1.00f);
    c[ImGuiCol_SeparatorHovered] = Accent;
    c[ImGuiCol_SeparatorActive] = Accent;
    c[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    c[ImGuiCol_ResizeGripHovered] = Accent;
    c[ImGuiCol_ResizeGripActive] = Accent;
    c[ImGuiCol_Tab] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    c[ImGuiCol_TabSelected] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    c[ImGuiCol_TabDimmed] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
    c[ImGuiCol_TabDimmedSelected] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    c[ImGuiCol_PlotLines] = Accent;
    c[ImGuiCol_PlotLinesHovered] = AccV(1.15f);
    c[ImGuiCol_PlotHistogram] = Accent;
    c[ImGuiCol_PlotHistogramHovered] = AccV(1.15f);
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.157f, 0.157f, 0.157f, 1.00f);
    c[ImGuiCol_TableBorderLight] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    c[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(Accent.x, Accent.y, Accent.z, 0.28f);
    c[ImGuiCol_DragDropTarget] = Accent;
    c[ImGuiCol_NavHighlight] = Accent;
    c[ImGuiCol_NavWindowingHighlight] = Accent;
    c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.45f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.55f);
}

} // namespace theme
