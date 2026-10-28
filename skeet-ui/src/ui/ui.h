#pragma once

#include "imgui.h"

// Host-agnostic menu renderer. Create an ImGui context + backend, call
// ui::Initialize(scale) once, then ui::Render() every frame.
namespace ui {

void Initialize(float scale);
void Shutdown();

void Render();

void SetVisible(bool visible);
bool IsVisible();

// True once the user clicked the custom close button.
bool ConsumeCloseRequest();

// Returns the accumulated title-bar drag delta in pixels for this frame.
bool TakeDrag(ImVec2& delta);

// Toast notification (drawn inside the menu).
void Toast(const char* title, const char* detail, bool good = true);

} // namespace ui
