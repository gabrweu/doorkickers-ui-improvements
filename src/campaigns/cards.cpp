// The campaign selection screen (Menu_CampaignSelect, stock campaign.xml). GameGUI::Campaign_Init clones
// #CampaignSelect_ItemTemplate into #CampaignSelect_List once per CampaignDef and names each card after the def's
// fileName. Finished runs are in g_campaignHistory (List<CampaignHistory>, saved as campaign_history.xml), which
// CampaignManager::End appends to; each entry has the campaign's fileName and the squad's name at the time.
//
// Every time the screen is shown, each card whose campaign is in the history gets:
//   - a badge (a clone of #dk2ml_cc_badge_template, the card's last child, so drawn on top): the campaign's medal and
//     "COMPLETED xN", bottom right, just above the card's name band;
//   - the list of squads that finished it (#dk2ml_cc_list_template) inside the card's #campaign_hover, which the card
//     Button's XML OnHover / OnHoverEnd show and hide: the list shows exactly while the card is hovered, no hook needed.
// Both are StaticImages (and so are their children), which never take the cursor: the card keeps its hover, its
// preview movie and its click. Our clones are found again by name inside each card, so nothing is kept per card and
// a card the game deletes takes them with it.
#include "campaigns/cards.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/game.h"
#include "core/gui.h"
#include "core/settings.h"

namespace {

constexpr const char* kScreen = "Menu_CampaignSelect";
constexpr const char* kList = "#CampaignSelect_List";
constexpr const char* kHover = "#campaign_hover";
constexpr const char* kBadgeTemplate = "#dk2ml_cc_badge_template";
constexpr const char* kListTemplate = "#dk2ml_cc_list_template";
constexpr const char* kBadge = "#dk2ml_cc_badge"; // the clones' names
constexpr const char* kSquads = "#dk2ml_cc_list";
constexpr const char* kMedal = "#dk2ml_cc_medal";
constexpr const char* kBadgeText = "#dk2ml_cc_badge_text";
constexpr const char* kListBg = "#dk2ml_cc_list_bg";
constexpr const char* kListTitle = "#dk2ml_cc_list_title";
constexpr const char* kListText = "#dk2ml_cc_list_text";

// CampaignHistory's char arrays (fileName[128], squadName[32])
constexpr size_t kFileNameLen = 128;
constexpr size_t kSquadNameLen = 32;

// the stock card (campaign.xml), from the card item's center, +y up
constexpr float kBandHalfW = 248;    // the bottom band with #type / #name: 496 wide
constexpr float kBandTop = 84;       // its top, above the card's bottom (align="b" origin="0 4", 80 high)
constexpr float kBannerBottom = 232; // #continue's banner, below the card's top (origin="0 -4", 228 high)
constexpr float kGap = 6;

// the badge
constexpr float kBadgePadX = 12;
constexpr float kMedalW = 100; // medals are 128 x 32
constexpr float kMedalH = 32;

// the list
constexpr float kBorder = 2;  // the orange frame (the list root) around the fill
constexpr float kPad = 12;
constexpr float kTitleH = 36; // title line + gap to the names

struct Squad {
    std::string name;
    int runs = 0;
};

struct Done {
    int runs = 0;
    std::vector<Squad> squads; // the most recent run's squad first
};

bool g_ready = false; // the items below were looked up for the current GUI (from GUI_LOADED on)
void* g_screen = nullptr;
void* g_list = nullptr;
void* g_badgeTemplate = nullptr;
void* g_listTemplate = nullptr;
bool g_loggedHistory = false;

// fileNames compare case-insensitively, either slash
std::string Key(const char* s, size_t max)
{
    std::string k(s, strnlen(s, max));
    for (char& c : k)
        c = c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return k;
}

std::unordered_map<std::string, Done> ReadHistory()
{
    std::unordered_map<std::string, Done> done;
    const uint8_t* list = game::g_campaignHistory.Address();
    int capacity = dk2ml::At<int>(list, game::List_CampaignHistory_m_size.Offset());
    int n = dk2ml::At<int>(list, game::List_CampaignHistory_m_elements.Offset());
    auto* entries = static_cast<const char*>(dk2ml::At<void*>(list, game::List_CampaignHistory_m_list.Offset()));
    if (n == 0)
        return done;
    if (n < 0 || n > capacity || capacity > 100000 || !entries) {
        if (!g_loggedHistory)
            game::api->Log("campaign history looks wrong (%d of %d, list %p), no campaign cards", n, capacity, entries);
        g_loggedHistory = true;
        return done;
    }
    uint32_t stride = game::sizeof_CampaignHistory.Get();
    for (int i = n; i-- > 0;) { // appended as runs end: newest last
        const char* e = entries + static_cast<size_t>(i) * stride;
        std::string file = Key(e + game::CampaignHistory_fileName.Offset(), kFileNameLen);
        if (file.empty())
            continue;
        const char* squadName = e + game::CampaignHistory_squadName.Offset();
        std::string squad(squadName, strnlen(squadName, kSquadNameLen));
        Done& d = done[file];
        d.runs++;
        auto it = std::find_if(d.squads.begin(), d.squads.end(), [&](const Squad& s) { return s.name == squad; });
        if (it == d.squads.end())
            d.squads.push_back({squad, 1});
        else
            it->runs++;
    }
    return done;
}

int* Size(void* item)
{
    return &dk2ml::At<int>(item, game::GUI_Item_m_size.Offset()); // {x, y}
}

// a StaticImage's drawn size (texture size x scale)
std::pair<float, float> Drawn(void* image)
{
    char* render = &dk2ml::At<char>(image, game::GUI_StaticImage_m_renderObject.Offset());
    const float* size = &dk2ml::At<float>(render, game::RenderObject2D_size.Offset());
    const float* scale = &dk2ml::At<float>(render, game::RenderObject2D_scale.Offset());
    return {size[0] * scale[0], size[1] * scale[1]};
}

// what the game does for its own cloned list items: re-resolve action targets; then our name, so the card finds it
void* Clone(void* tmpl, const char* name)
{
    void* clone = game::GUI_StaticImage_CloneInternal(tmpl);
    if (!clone)
        return nullptr;
    game::GUI_Item_UpdatePointerReferences(clone, clone, game::GuiRoot());
    game::GUI_Item_SetName(clone, name);
    return clone;
}

// The badge: the medal (if the campaign has one) and "COMPLETED xN", its width fitted to them, right-aligned with the
// card's name band. Returns its top (from the card's center).
float FillBadge(void* badge, const char* fileName, const Done& done, float cardH)
{
    void* medal = gui::Find(badge, kMedal);
    void* text = gui::Find(badge, kBadgeText);
    const void* def = game::CampaignManager_GetDef(fileName);
    const char* medalImage = def ? &dk2ml::At<char>(def, game::CampaignDef_medalImage.Offset()) : nullptr;
    bool showMedal = medalImage && *medalImage && gui::IsA(medal, game::GUI_StaticImage_vftable.Address());
    float medalW = 0;
    if (showMedal) {
        game::GUI_StaticImage_SetImageAndImplicitSize(medal, medalImage);
        gui::FitImage(medal, kMedalW, kMedalH, 1.0f);
        medalW = Drawn(medal).first;
    }
    gui::ShowIf(medal, showMedal);

    std::string s = done.runs > 1 ? "COMPLETED x" + std::to_string(done.runs) : "COMPLETED";
    gui::SetTextMeasured(text, s);
    float textW = text ? static_cast<float>(Size(text)[0]) : 0; // no sizeX: ChangeText stored the width

    float h = Drawn(badge).second;
    float w = std::ceil(kBadgePadX + (showMedal ? medalW + kGap : 0) + textW + kBadgePadX);
    gui::StretchTo(badge, w, 0);
    Size(badge)[0] = static_cast<int>(w);
    Size(badge)[1] = static_cast<int>(h);
    if (showMedal)
        gui::SetOrigin(medal, -w / 2 + kBadgePadX + medalW / 2, 0);
    if (text)
        gui::SetOrigin(text, -kBadgePadX, 0); // align="r": from the right edge, re-placed for the new width
    float bottom = -cardH / 2 + kBandTop + kGap;
    gui::SetOrigin(badge, kBandHalfW - w / 2, bottom + h / 2);
    return bottom + h;
}

// One line per squad, as many as fit in maxH, the rest as "+N MORE". Returns the text's height.
float SetLines(void* text, const std::vector<Squad>& squads, float maxH)
{
    auto join = [&](size_t n) {
        std::string s;
        for (size_t i = 0; i < n; i++) {
            s += (i ? "\n" : "") + (squads[i].name.empty() ? std::string("(NO NAME)") : squads[i].name);
            if (squads[i].runs > 1)
                s += " x" + std::to_string(squads[i].runs);
        }
        if (n < squads.size())
            s += "\n+" + std::to_string(squads.size() - n) + " MORE";
        return s;
    };
    float h = gui::SetTextMeasured(text, join(squads.size()));
    for (size_t n = squads.size(); h > maxH && n > 1;)
        h = gui::SetTextMeasured(text, join(--n));
    return h;
}

// The squads list, its height fitted to the names, from just above the badge up to (at most) below the CONTINUE
// banner. Parented to the card's #campaign_hover, which is centered on the card.
void FillList(void* list, const Done& done, float bottom, float cardH)
{
    void* text = gui::Find(list, kListText);
    void* title = gui::Find(list, kListTitle);
    float textY = kBorder + kPad + kTitleH; // from the list's top
    float maxH = cardH / 2 - kBannerBottom - kGap - bottom;
    float maxText = std::max(30.0f, maxH - textY - kPad - kBorder);
    float h = std::ceil(textY + SetLines(text, done.squads, maxText) + kPad + kBorder);

    gui::StretchTo(list, 0, h);
    Size(list)[1] = static_cast<int>(h);
    gui::StretchTo(gui::Find(list, kListBg), 0, h - 2 * kBorder);
    if (title)
        gui::SetOrigin(title, 0, -(kBorder + kPad)); // align="t": from the top, re-placed for the new height
    if (text)
        gui::SetOrigin(text, 0, -textY);
    gui::SetOrigin(list, 0, bottom + h / 2);
}

void Refresh()
{
    if (!g_ready || !g_list)
        return;
    bool on = g_settings.campaignCards && game::CampaignCardsUsable() && g_badgeTemplate;
    std::unordered_map<std::string, Done> done;
    if (on)
        done = ReadHistory();
    for (void* card : gui::Children(g_list)) {
        const char* name = gui::Name(card); // the CampaignDef's fileName
        if (!name || !*name)
            continue;
        auto it = done.find(Key(name, kFileNameLen));
        void* badge = gui::Find(card, kBadge);
        void* list = gui::Find(card, kSquads);
        if (it == done.end()) {
            gui::ShowIf(badge, false);
            gui::ShowIf(list, false);
            continue;
        }
        if (!badge) {
            badge = Clone(g_badgeTemplate, kBadge);
            if (!badge)
                continue;
            gui::AddChild(card, badge); // last: drawn on top of the card
        }
        void* hover = gui::Find(card, kHover);
        if (!list && hover && g_listTemplate) {
            list = Clone(g_listTemplate, kSquads);
            if (list)
                gui::AddChild(hover, list);
        }
        float cardH = static_cast<float>(Size(card)[1]);
        float badgeTop = FillBadge(badge, name, it->second, cardH);
        gui::ShowIf(badge, true);
        if (list) {
            FillList(list, it->second, badgeTop + kGap, cardH);
            gui::ShowIf(list, true); // shown with #campaign_hover
        }
    }
}

} // namespace

void Campaigns_OnShow(const void* item)
{
    if (item == g_screen && g_screen)
        Refresh();
}

void Campaigns_OnGuiLoaded()
{
    // the old items are gone (their addresses may be reused): find the new ones
    void* root = game::GuiRoot();
    g_screen = gui::Find(root, kScreen);
    g_list = gui::Find(g_screen, kList);
    g_badgeTemplate = gui::Find(root, kBadgeTemplate);
    g_listTemplate = gui::Find(root, kListTemplate);
    // our clones are StaticImages (StaticImage::CloneInternal)
    if (!gui::IsA(g_badgeTemplate, game::GUI_StaticImage_vftable.Address()))
        g_badgeTemplate = nullptr;
    if (!gui::IsA(g_listTemplate, game::GUI_StaticImage_vftable.Address()))
        g_listTemplate = nullptr;
    g_ready = true;
    if (!g_list)
        game::api->Log("campaign cards: %s > %s not found (a mod replaced campaign.xml?)", kScreen, kList);
    if (!g_badgeTemplate || !g_listTemplate)
        game::api->Log("gui/campaign_cards.xml: %s / %s missing or not a StaticImage", kBadgeTemplate, kListTemplate);
}

void Campaigns_SetEnabled(bool)
{
    Refresh(); // shows or hides every card's badge and list
}
