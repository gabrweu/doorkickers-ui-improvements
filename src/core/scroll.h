// Scroll positions for the grouped, virtualized lists (the equipment grid, the portrait picker). Their rows are group
// headers or card rows, and a list whose top row is a group's header looks exactly like one whose top is that group's
// first card row (the header sticks to the top). So headers aren't scroll stops: every wheel notch moves the same
// number of card rows, instead of sometimes spending a step on a header. A header with no card row after it (a
// collapsed group in the equipment grid) is a stop: there's nothing it would look like.
#pragma once

#include <cstdlib>

namespace scroll {

// Whether row `i` is a scroll stop: a card row, or a header not followed by a card row.
template <typename Rows>
bool IsStop(const Rows& rows, int i)
{
    int size = static_cast<int>(rows.size());
    return !rows[i].header || i + 1 >= size || rows[i + 1].header;
}

// The stop at `top`: a header row followed by its group's first card row becomes that row.
template <typename Rows>
int Stop(const Rows& rows, int top)
{
    int size = static_cast<int>(rows.size());
    if (top >= 0 && top < size && !IsStop(rows, top))
        top++;
    return top < 0 ? 0 : top;
}

// `cardRows` stops down (positive) or up (negative) from `top`, skipping headers that aren't stops; stops at either end.
template <typename Rows>
int Step(const Rows& rows, int top, int cardRows)
{
    int size = static_cast<int>(rows.size());
    int dir = cardRows < 0 ? -1 : 1;
    int t = Stop(rows, top);
    for (int i = 0; i < std::abs(cardRows); i++) {
        int next = t + dir;
        while (next >= 0 && next < size && !IsStop(rows, next))
            next += dir;
        if (next < 0 || next >= size)
            break;
        t = next;
    }
    return t;
}

} // namespace scroll
