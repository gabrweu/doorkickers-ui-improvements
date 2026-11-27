#include "core/gui.h"

#include <algorithm>
#include <cstdio>

#include "core/game.h"

namespace gui {

void* Find(void* parent, const char* name)
{
    return parent ? dk2ml::gui::Find(game::api, name, parent) : nullptr; // (the kit takes NULL as the whole GUI)
}

bool IsA(const void* item, const void* vtbl)
{
    return item && *static_cast<const void* const*>(item) == vtbl;
}

bool IsHidden(const void* item)
{
    return !dk2ml::gui::IsShown(game::api, const_cast<void*>(item));
}

void* Parent(const void* item)
{
    return dk2ml::gui::Parent(game::api, const_cast<void*>(item));
}

std::vector<void*> Children(const void* item)
{
    void* self = const_cast<void*>(item);
    std::vector<void*> children(64);
    int n = dk2ml::gui::Children(game::api, self, children.data(), static_cast<int>(children.size()));
    if (n > static_cast<int>(children.size())) { // more than we guessed: ask again with room for all
        children.resize(n);
        n = dk2ml::gui::Children(game::api, self, children.data(), n);
    }
    children.resize(std::min<size_t>(static_cast<size_t>(n), children.size()));
    return children;
}

const char* Name(const void* item)
{
    return dk2ml::gui::Name(game::api, const_cast<void*>(item));
}

const char* Text(const void* item)
{
    return IsA(item, game::GUI_StaticText_vftable.Address()) ? dk2ml::At<const char*>(item, game::GUI_StaticText_m_text.Offset()) : nullptr;
}

void ShowIf(void* item, bool show)
{
    if (item)
        dk2ml::gui::Show(game::api, item, show);
}

void SetTextColor(void* item, unsigned color)
{
    if (IsA(item, game::GUI_StaticText_vftable.Address()))
        dk2ml::At<unsigned>(item, game::GUI_StaticText_m_textRenderObject.Offset() + game::GUI_sTextRenderObject_color.Offset()) = color;
}

unsigned TextColor(const void* item)
{
    return IsA(item, game::GUI_StaticText_vftable.Address()) ? dk2ml::At<unsigned>(item, game::GUI_StaticText_m_textRenderObject.Offset() + game::GUI_sTextRenderObject_color.Offset()) : 0;
}

namespace {

int TextHeight(void* label, const char* text)
{
    game::GUI_StaticText_ChangeText(label, text);
    return dk2ml::At<int>(label, game::GUI_Item_m_size.Offset() + static_cast<int32_t>(sizeof(int))); // m_size {x, y}
}

int TextWidth(void* label, const char* text)
{
    game::GUI_StaticText_ChangeText(label, text);
    return dk2ml::At<int>(label, game::GUI_Item_m_size.Offset()); // m_size.x: no sizeX, so the laid-out width
}

} // namespace

float SetTextMeasured(void* text, const std::string& s)
{
    return IsA(text, game::GUI_StaticText_vftable.Address()) ? static_cast<float>(TextHeight(text, s.c_str())) : 0;
}

void StretchTo(void* image, float w, float h)
{
    if (!IsA(image, game::GUI_StaticImage_vftable.Address()))
        return;
    char* render = &dk2ml::At<char>(image, game::GUI_StaticImage_m_renderObject.Offset());
    const float* size = &dk2ml::At<float>(render, game::RenderObject2D_size.Offset());
    float* scale = &dk2ml::At<float>(render, game::RenderObject2D_scale.Offset());
    if (w > 0 && size[0] > 0)
        scale[0] = w / size[0];
    if (h > 0 && size[1] > 0)
        scale[1] = h / size[1];
    dk2ml::At<bool>(render, game::RenderObject2D_bNeedsUpdate.Offset()) = true;
}

void SetTextFitted(void* label, const std::string& text, TextFit& fit, int lines)
{
    if (!IsA(label, game::GUI_StaticText_vftable.Address()))
        return;
    if (fit.oneLine == 0) {
        fit.oneLine = TextHeight(label, "X");
        fit.twoLines = TextHeight(label, "X\nX");
        if (fit.oneLine <= 0 || fit.twoLines <= fit.oneLine)
            fit.oneLine = -1; // can't measure: texts are set as they are
    }
    if (fit.oneLine < 0) {
        game::GUI_StaticText_ChangeText(label, text.c_str());
        return;
    }
    int line = fit.twoLines - fit.oneLine;
    int limit = (lines <= 1 ? fit.oneLine : fit.twoLines) + line / 2; // halfway to the next line
    if (TextHeight(label, text.c_str()) <= limit)
        return;

    std::vector<size_t> spaces; // where each word after the first starts, minus one
    for (size_t i = 1; i < text.size(); i++)
        if (text[i] == ' ' && text[i - 1] != ' ')
            spaces.push_back(i);
    int tries = 0;
    for (size_t k = spaces.size(); k-- > 0 && tries < 40; tries++) {
        std::string s = text.substr(0, spaces[k]) + "...";
        if (TextHeight(label, s.c_str()) <= limit)
            return;
    }
    // the first word alone doesn't fit: cut it
    size_t len = spaces.empty() ? text.size() : spaces[0];
    for (; len > 1 && tries < 80; len--, tries++) {
        std::string s = text.substr(0, len - 1) + "...";
        if (TextHeight(label, s.c_str()) <= limit)
            return;
    }
}

void SetTextFittedWidth(void* label, const std::string& text, float maxW)
{
    if (!IsA(label, game::GUI_StaticText_vftable.Address()))
        return;
    if (TextWidth(label, text.c_str()) <= maxW)
        return;
    std::vector<size_t> spaces; // where each word after the first starts, minus one
    for (size_t i = 1; i < text.size(); i++)
        if (text[i] == ' ' && text[i - 1] != ' ')
            spaces.push_back(i);
    int tries = 0;
    for (size_t k = spaces.size(); k-- > 0 && tries < 40; tries++) {
        std::string s = text.substr(0, spaces[k]) + "...";
        if (TextWidth(label, s.c_str()) <= maxW)
            return;
    }
    // the first word alone doesn't fit: cut it
    size_t len = spaces.empty() ? text.size() : spaces[0];
    for (; len > 1 && tries < 80; len--, tries++) {
        std::string s = text.substr(0, len - 1) + "...";
        if (TextWidth(label, s.c_str()) <= maxW)
            return;
    }
}

void FitImage(void* image, float boxW, float boxH, float maxScale)
{
    if (!IsA(image, game::GUI_StaticImage_vftable.Address()) || maxScale <= 0 || boxW <= 0 || boxH <= 0)
        return;
    // SetImageAndImplicitSize wrote the texture's pixel size into the inline RenderObject2D and kept its scale
    char* render = &dk2ml::At<char>(image, game::GUI_StaticImage_m_renderObject.Offset());
    const float* size = &dk2ml::At<float>(render, game::RenderObject2D_size.Offset());
    if (size[0] <= 0 || size[1] <= 0)
        return;
    float s = maxScale;
    if (boxW / size[0] < s)
        s = boxW / size[0];
    if (boxH / size[1] < s)
        s = boxH / size[1];
    float* scale = &dk2ml::At<float>(render, game::RenderObject2D_scale.Offset());
    if (scale[0] == s && scale[1] == s)
        return;
    scale[0] = scale[1] = s;
    dk2ml::At<bool>(render, game::RenderObject2D_bNeedsUpdate.Offset()) = true;
}

void AddChild(void* parent, void* item)
{
    dk2ml::gui::AddChild(game::api, parent, item);
}

void SetOrigin(void* item, float x, float y)
{
    dk2ml::gui::SetOrigin(game::api, item, x, y);
}

void ShiftBy(void* item, float dy)
{
    dk2ml::At<float>(item, game::GUI_Item_m_alignOffset.Offset() + static_cast<int32_t>(sizeof(float))) += dy;
    dk2ml::At<float>(item, game::GUI_Item_m_localOrigin.Offset() + static_cast<int32_t>(sizeof(float))) += dy;
}

float OffsetY(void* item)
{
    return dk2ml::At<float>(item, game::GUI_Item_m_alignOffset.Offset() + static_cast<int32_t>(sizeof(float)));
}

void SetCallback(void* item, int64_t event, Callback fn)
{
    dk2ml::gui::OnAction(game::api, item, event, fn);
}

} // namespace gui
