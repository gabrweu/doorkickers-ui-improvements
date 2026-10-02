#include "gui.h"

#include <cstdio>

#include "game.h"

namespace gui {

namespace {

// Runs a hand-built GUI::sAction, the same as an XML <Action> owned by `owner`. params must outlive the call.
void Run(int64_t type, void* owner, void* target, const char* params)
{
    std::vector<char> action(game::off.actionSize, 0);
    game::Field<void*>(action.data(), game::off.actionOwner) = owner;
    game::Field<int>(action.data(), game::off.actionType) = static_cast<int>(type);
    game::Field<const char*>(action.data(), game::off.actionTargetName) = "";
    game::Field<void*>(action.data(), game::off.actionTarget) = target;
    game::Field<const char*>(action.data(), game::off.actionParams) = params;
    game::fn.actionExecute(action.data());
}

} // namespace

void* Find(void* parent, const char* name)
{
    return parent ? game::fn.findChild(parent, name) : nullptr;
}

bool IsA(const void* item, const void* vtbl)
{
    return item && *static_cast<const void* const*>(item) == vtbl;
}

bool IsHidden(const void* item)
{
    return game::Field<bool>(item, game::off.itemHidden);
}

void* Parent(const void* item)
{
    return game::Field<void*>(item, game::off.itemParent);
}

std::vector<void*> Children(const void* item)
{
    // m_children is the head node of an intrusive circular list; each child's own node (a LinkedList<GUI::Item> base)
    // points back to the child through `owner`. Walked the way GUI::ItemList::Init walks it.
    std::vector<void*> children;
    const void* list = &game::Field<char>(item, game::off.itemChildren);
    const void* head = game::Field<void*>(list, game::off.linkHead);
    const void* node = game::Field<void*>(list, game::off.linkNext);
    for (int guard = 0; node && node != head && guard < 100000; guard++) {
        if (void* child = game::Field<void*>(node, game::off.linkOwner))
            children.push_back(child);
        node = game::Field<void*>(node, game::off.linkNext);
    }
    return children;
}

const char* Name(const void* item)
{
    const char* name = game::Field<const char*>(item, game::off.itemName + game::off.hashedStringText);
    return name ? name : "";
}

const char* Text(const void* item)
{
    return IsA(item, game::globals.staticTextVtbl) ? game::Field<const char*>(item, game::off.staticTextText) : nullptr;
}

void ShowIf(void* item, bool show)
{
    if (!item)
        return;
    if (show)
        game::fn.itemShow(item);
    else
        game::fn.itemHide(item);
}

void SetTextColor(void* item, unsigned color)
{
    if (IsA(item, game::globals.staticTextVtbl))
        game::Field<unsigned>(item, game::off.staticTextRender + game::off.textRenderColor) = color;
}

unsigned TextColor(const void* item)
{
    return IsA(item, game::globals.staticTextVtbl) ? game::Field<unsigned>(item, game::off.staticTextRender + game::off.textRenderColor) : 0;
}

namespace {

int TextHeight(void* label, const char* text)
{
    game::fn.staticTextChangeText(label, text);
    return game::Field<int>(label, game::off.itemSize + static_cast<int32_t>(sizeof(int))); // m_size {x, y}
}

} // namespace

void SetTextFitted(void* label, const std::string& text, TextFit& fit)
{
    if (!IsA(label, game::globals.staticTextVtbl))
        return;
    if (fit.oneLine == 0) {
        fit.oneLine = TextHeight(label, "X");
        fit.twoLines = TextHeight(label, "X\nX");
        if (fit.oneLine <= 0 || fit.twoLines <= fit.oneLine)
            fit.oneLine = -1; // can't measure: texts are set as they are
    }
    if (fit.oneLine < 0) {
        game::fn.staticTextChangeText(label, text.c_str());
        return;
    }
    int limit = fit.twoLines + (fit.twoLines - fit.oneLine) / 2; // halfway to a third line
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

void FitImage(void* image, float boxW, float boxH, float maxScale)
{
    if (!IsA(image, game::globals.staticImageVtbl) || maxScale <= 0 || boxW <= 0 || boxH <= 0)
        return;
    // SetImageAndImplicitSize wrote the texture's pixel size into the inline RenderObject2D and kept its scale
    char* render = &game::Field<char>(image, game::off.staticImageRender);
    const float* size = &game::Field<float>(render, game::off.renderSize);
    if (size[0] <= 0 || size[1] <= 0)
        return;
    float s = maxScale;
    if (boxW / size[0] < s)
        s = boxW / size[0];
    if (boxH / size[1] < s)
        s = boxH / size[1];
    float* scale = &game::Field<float>(render, game::off.renderScale);
    if (scale[0] == s && scale[1] == s)
        return;
    scale[0] = scale[1] = s;
    game::Field<bool>(render, game::off.renderNeedsUpdate) = true;
}

void AddChild(void* parent, void* item)
{
    Run(game::off.actionAddChild, parent, item, nullptr);
}

void SetOrigin(void* item, float x, float y)
{
    static char params[64]; // SetOrigin parses it with "%f %f" during the call
    snprintf(params, sizeof(params), "%.1f %.1f", x, y);
    Run(game::off.actionSetOrigin, item, item, params);
}

void SetCallback(void* item, int64_t event, Callback fn)
{
    char* props = &game::Field<char>(item, game::off.itemEvents + static_cast<int32_t>(event * game::off.eventPropsSize));
    char* list = props + game::off.eventPropsActions;
    void** actions = game::Field<void**>(list, game::off.actionListData);
    int count = game::Field<int>(list, game::off.actionListCount);
    for (int i = 0; i < count; i++) {
        void* a = actions[i];
        if (a && game::Field<int>(a, game::off.actionType) == game::off.actionCallbackType)
            game::Field<Callback>(a, game::off.actionCallback) = fn;
    }
}

void* ActionOwner(const void* action)
{
    return game::Field<void*>(action, game::off.actionOwner);
}

float ActionCursorY(const void* action)
{
    // Vector2 {x, y}
    return game::Field<float>(action, game::off.actionEventParams + game::off.eventParamsCursor + static_cast<int32_t>(sizeof(float)));
}

} // namespace gui
