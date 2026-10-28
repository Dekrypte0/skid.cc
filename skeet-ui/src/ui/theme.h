#pragma once

#include "imgui.h"

namespace theme {

// Global dpi scale — every layout constant in the UI is multiplied by this.
extern float S;

// Live accent (lime by default, editable from the menu).
extern ImVec4 Accent;

extern ImFont* FontMain;
extern ImFont* FontBold;
extern ImFont* FontSmall;
extern ImFont* FontSmallBold;
extern ImFont* FontTitle;

extern float SizeMain;
extern float SizeBold;
extern float SizeSmall;
extern float SizeSmallBold;
extern float SizeTitle;

inline float Px(float v) { return v * S; }

// Palette (hex, no alpha).
namespace col {
inline constexpr unsigned Bg0        = 0x0A0A0A;
inline constexpr unsigned Bg1        = 0x101010;
inline constexpr unsigned Sidebar    = 0x0B0B0B;
inline constexpr unsigned Header     = 0x0C0C0C;
inline constexpr unsigned Group      = 0x171717;
inline constexpr unsigned GroupAlt   = 0x141414;
inline constexpr unsigned Border     = 0x282828;
inline constexpr unsigned BorderDim  = 0x1E1E1E;
inline constexpr unsigned Black      = 0x050505;
inline constexpr unsigned Text       = 0xCCCCCC;
inline constexpr unsigned TextBright = 0xF0F0F0;
inline constexpr unsigned Dim        = 0x6E6E6E;
inline constexpr unsigned Faint      = 0x4A4A4A;
inline constexpr unsigned RowHover   = 0x1D1D1D;
inline constexpr unsigned Track      = 0x262626;
inline constexpr unsigned Field      = 0x1A1A1A;
inline constexpr unsigned FieldHover = 0x222222;
inline constexpr unsigned Red        = 0xE05A5A;
inline constexpr unsigned Green      = 0x8FCB5C;
} // namespace col

ImU32 Hex(unsigned rgb, int a = 255);
ImU32 Acc(float k = 1.0f, int a = 255);
ImVec4 AccV(float k = 1.0f, float a = 1.0f);

void SetAccent(const ImVec4& accent);
void Init(float scale);
void ApplyStyle();

} // namespace theme
