#pragma once

#include "imgui.h"

namespace icons {

enum class Id {
    Home,
    Rage,
    Visuals,
    Misc,
    Skins,
    Players,
    Config,
    Search,
    Close,
    Check,
    ChevronDown,
    Save,
    Folder,
    Trash,
    Key,
    Info,
    Warning,
    Plus,
    Skull,
    Dot,
};

// Draws a vector icon centered at `center` inside a `size` x `size` box.
void Draw(ImDrawList* dl, Id id, ImVec2 center, float size, ImU32 col, float thickness = 0.0f);

} // namespace icons
