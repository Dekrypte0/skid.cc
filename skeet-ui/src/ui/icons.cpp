#include "icons.h"

#include "imgui_internal.h"

#include <cmath>

namespace icons {

namespace {

void Line(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float th)
{
    dl->AddLine(a, b, col, th);
}

void Polyline(ImDrawList* dl, const ImVec2* pts, int count, ImU32 col, float th, bool closed = false)
{
    for (int i = 0; i + 1 < count; ++i)
        dl->AddLine(pts[i], pts[i + 1], col, th);
    if (closed)
        dl->AddLine(pts[count - 1], pts[0], col, th);
}

} // namespace

void Draw(ImDrawList* dl, Id id, ImVec2 c, float size, ImU32 col, float thickness)
{
    if (!dl || size <= 0.0f)
        return;

    const float s = size;
    const float th = thickness > 0.0f ? thickness : ImMax(1.0f, s * 0.075f);

    switch (id)
    {
    case Id::Rage:
    {
        const float r = s * 0.28f;
        dl->AddCircle(c, r, col, 0, th);
        const float gap = r + s * 0.07f;
        const float ext = s * 0.48f;
        Line(dl, ImVec2(c.x - ext, c.y), ImVec2(c.x - gap, c.y), col, th);
        Line(dl, ImVec2(c.x + gap, c.y), ImVec2(c.x + ext, c.y), col, th);
        Line(dl, ImVec2(c.x, c.y - ext), ImVec2(c.x, c.y - gap), col, th);
        Line(dl, ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + ext), col, th);
        break;
    }
    case Id::Visuals:
    {
        const ImVec2 rad(s * 0.47f, s * 0.29f);
        dl->AddEllipse(c, rad, col, 0.0f, 40, th);
        dl->AddCircleFilled(c, s * 0.125f, col);
        break;
    }
    case Id::Misc:
    {
        const float r = s * 0.27f;
        dl->AddCircle(c, r, col, 0, th);
        dl->AddCircle(c, s * 0.10f, col, 0, th);
        for (int i = 0; i < 8; ++i)
        {
            const float a = (float)i * 3.14159265f * 0.25f;
            const ImVec2 p0(c.x + std::cos(a) * r, c.y + std::sin(a) * r);
            const ImVec2 p1(c.x + std::cos(a) * (r + s * 0.11f), c.y + std::sin(a) * (r + s * 0.11f));
            Line(dl, p0, p1, col, th);
        }
        break;
    }
    case Id::Skins:
    {
        // dagger: blade, guard, grip
        const ImVec2 blade[] = {
            ImVec2(c.x, c.y - s * 0.46f),
            ImVec2(c.x - s * 0.13f, c.y - s * 0.12f),
            ImVec2(c.x + s * 0.13f, c.y - s * 0.12f)
        };
        Polyline(dl, blade, 3, col, th, true);
        Line(dl, ImVec2(c.x - s * 0.27f, c.y - s * 0.06f),
             ImVec2(c.x + s * 0.27f, c.y - s * 0.06f), col, th);
        dl->AddRect(ImVec2(c.x - s * 0.065f, c.y - s * 0.01f),
                    ImVec2(c.x + s * 0.065f, c.y + s * 0.32f),
                    col, s * 0.05f, 0, th);
        break;
    }
    case Id::Players:
    {
        const ImVec2 h1(c.x - s * 0.15f, c.y - s * 0.13f);
        const ImVec2 h2(c.x + s * 0.16f, c.y - s * 0.09f);
        dl->AddCircle(h1, s * 0.115f, col, 0, th);
        dl->AddCircle(h2, s * 0.10f, col, 0, th);
        dl->PathArcTo(ImVec2(h1.x, h1.y + s * 0.24f), s * 0.24f, 3.14159265f, 6.2831853f, 16);
        dl->PathStroke(col, 0, th);
        dl->PathArcTo(ImVec2(h2.x, h2.y + s * 0.28f), s * 0.24f, 3.14159265f, 6.2831853f, 16);
        dl->PathStroke(col, 0, th);
        break;
    }
    case Id::Config:
    {
        const float x0 = c.x - s * 0.38f;
        const float x1 = c.x + s * 0.38f;
        const float ys[3] = { c.y - s * 0.22f, c.y, c.y + s * 0.22f };
        const float kn[3] = { c.x - s * 0.12f, c.x + s * 0.18f, c.x - s * 0.02f };
        for (int i = 0; i < 3; ++i)
        {
            Line(dl, ImVec2(x0, ys[i]), ImVec2(x1, ys[i]), col, th);
            dl->AddCircleFilled(ImVec2(kn[i], ys[i]), s * 0.075f, col);
        }
        break;
    }
    case Id::Search:
    {
        const ImVec2 cc(c.x - s * 0.08f, c.y - s * 0.08f);
        dl->AddCircle(cc, s * 0.24f, col, 0, th);
        Line(dl, ImVec2(cc.x + s * 0.19f, cc.y + s * 0.19f), ImVec2(c.x + s * 0.36f, c.y + s * 0.36f), col, th);
        break;
    }
    case Id::Close:
    {
        const float r = s * 0.26f;
        Line(dl, ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, th);
        Line(dl, ImVec2(c.x + r, c.y - r), ImVec2(c.x - r, c.y + r), col, th);
        break;
    }
    case Id::Check:
    {
        const ImVec2 p[] = {
            ImVec2(c.x - s * 0.30f, c.y + s * 0.02f),
            ImVec2(c.x - s * 0.08f, c.y + s * 0.24f),
            ImVec2(c.x + s * 0.32f, c.y - s * 0.24f)
        };
        Polyline(dl, p, 3, col, th);
        break;
    }
    case Id::ChevronDown:
    {
        const ImVec2 p[] = {
            ImVec2(c.x - s * 0.28f, c.y - s * 0.12f),
            ImVec2(c.x, c.y + s * 0.16f),
            ImVec2(c.x + s * 0.28f, c.y - s * 0.12f)
        };
        Polyline(dl, p, 3, col, th);
        break;
    }
    case Id::Save:
    {
        const ImVec2 a(c.x - s * 0.32f, c.y - s * 0.32f);
        const ImVec2 b(c.x + s * 0.32f, c.y + s * 0.32f);
        dl->AddRect(a, b, col, ImMax(1.0f, s * 0.05f), 0, th);
        dl->AddRect(ImVec2(c.x - s * 0.13f, c.y - s * 0.32f),
                    ImVec2(c.x + s * 0.18f, c.y - s * 0.06f), col, 0, 0, th);
        dl->AddRect(ImVec2(c.x - s * 0.18f, c.y + s * 0.10f),
                    ImVec2(c.x + s * 0.18f, c.y + s * 0.32f), col, 0, 0, th);
        break;
    }
    case Id::Folder:
    {
        const ImVec2 p[] = {
            ImVec2(c.x - s * 0.36f, c.y - s * 0.14f),
            ImVec2(c.x - s * 0.16f, c.y - s * 0.30f),
            ImVec2(c.x - s * 0.01f, c.y - s * 0.30f),
            ImVec2(c.x + s * 0.05f, c.y - s * 0.14f),
            ImVec2(c.x + s * 0.36f, c.y - s * 0.14f),
            ImVec2(c.x + s * 0.30f, c.y + s * 0.28f),
            ImVec2(c.x - s * 0.36f, c.y + s * 0.28f)
        };
        Polyline(dl, p, 7, col, th, true);
        break;
    }
    case Id::Trash:
    {
        Line(dl, ImVec2(c.x - s * 0.32f, c.y - s * 0.22f), ImVec2(c.x + s * 0.32f, c.y - s * 0.22f), col, th);
        Line(dl, ImVec2(c.x - s * 0.10f, c.y - s * 0.32f), ImVec2(c.x + s * 0.10f, c.y - s * 0.32f), col, th);
        const ImVec2 p[] = {
            ImVec2(c.x - s * 0.24f, c.y - s * 0.16f),
            ImVec2(c.x + s * 0.24f, c.y - s * 0.16f),
            ImVec2(c.x + s * 0.18f, c.y + s * 0.32f),
            ImVec2(c.x - s * 0.18f, c.y + s * 0.32f)
        };
        Polyline(dl, p, 4, col, th, true);
        Line(dl, ImVec2(c.x - s * 0.06f, c.y - s * 0.06f), ImVec2(c.x - s * 0.06f, c.y + s * 0.20f), col, th);
        Line(dl, ImVec2(c.x + s * 0.08f, c.y - s * 0.06f), ImVec2(c.x + s * 0.08f, c.y + s * 0.20f), col, th);
        break;
    }
    case Id::Key:
    {
        dl->AddCircle(ImVec2(c.x - s * 0.20f, c.y), s * 0.13f, col, 0, th);
        Line(dl, ImVec2(c.x - s * 0.07f, c.y), ImVec2(c.x + s * 0.34f, c.y), col, th);
        Line(dl, ImVec2(c.x + s * 0.22f, c.y), ImVec2(c.x + s * 0.22f, c.y + s * 0.14f), col, th);
        Line(dl, ImVec2(c.x + s * 0.32f, c.y), ImVec2(c.x + s * 0.32f, c.y + s * 0.18f), col, th);
        break;
    }
    case Id::Info:
    {
        dl->AddCircle(c, s * 0.34f, col, 0, th);
        dl->AddCircleFilled(ImVec2(c.x, c.y - s * 0.16f), s * 0.045f, col);
        Line(dl, ImVec2(c.x, c.y - s * 0.04f), ImVec2(c.x, c.y + s * 0.17f), col, th);
        break;
    }
    case Id::Warning:
    {
        const ImVec2 p[] = {
            ImVec2(c.x, c.y - s * 0.34f),
            ImVec2(c.x + s * 0.36f, c.y + s * 0.30f),
            ImVec2(c.x - s * 0.36f, c.y + s * 0.30f)
        };
        Polyline(dl, p, 3, col, th, true);
        Line(dl, ImVec2(c.x, c.y - s * 0.10f), ImVec2(c.x, c.y + s * 0.10f), col, th);
        dl->AddCircleFilled(ImVec2(c.x, c.y + s * 0.20f), s * 0.035f, col);
        break;
    }
    case Id::Plus:
    {
        Line(dl, ImVec2(c.x - s * 0.27f, c.y), ImVec2(c.x + s * 0.27f, c.y), col, th);
        Line(dl, ImVec2(c.x, c.y - s * 0.27f), ImVec2(c.x, c.y + s * 0.27f), col, th);
        break;
    }
    case Id::Skull:
    {
        dl->AddCircle(ImVec2(c.x, c.y - s * 0.06f), s * 0.30f, col, 0, th);
        const ImVec2 jaw[] = {
            ImVec2(c.x - s * 0.16f, c.y + s * 0.22f),
            ImVec2(c.x + s * 0.16f, c.y + s * 0.22f),
            ImVec2(c.x + s * 0.13f, c.y + s * 0.34f),
            ImVec2(c.x - s * 0.13f, c.y + s * 0.34f)
        };
        Polyline(dl, jaw, 4, col, th, true);
        dl->AddCircleFilled(ImVec2(c.x - s * 0.12f, c.y - s * 0.08f), s * 0.06f, col);
        dl->AddCircleFilled(ImVec2(c.x + s * 0.12f, c.y - s * 0.08f), s * 0.06f, col);
        break;
    }
    case Id::Dot:
    {
        dl->AddCircleFilled(c, s * 0.18f, col);
        break;
    }
    }
}

} // namespace icons
