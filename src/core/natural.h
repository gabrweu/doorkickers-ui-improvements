// "Natural" text order, shared by the portrait picker (file names) and the equipment grid (item names): case-insensitive,
// digit runs compared as numbers, so ranger2 < ranger10 and G3 < G36.
#pragma once

namespace natural {

inline int Fold(char c)
{
    return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : static_cast<unsigned char>(c);
}

inline bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

// < 0, 0 or > 0 as a sorts before, with or after b. 0 doesn't mean identical ("Ak" and "AK", "7" and "07"): callers
// that need a total order break ties themselves.
inline int Compare(const char* a, const char* b)
{
    while (*a && *b) {
        if (IsDigit(*a) && IsDigit(*b)) {
            while (*a == '0')
                a++;
            while (*b == '0')
                b++;
            const char* ea = a;
            const char* eb = b;
            while (IsDigit(*ea))
                ea++;
            while (IsDigit(*eb))
                eb++;
            if (ea - a != eb - b)
                return ea - a < eb - b ? -1 : 1; // fewer digits = smaller number
            for (; a < ea; a++, b++)
                if (*a != *b)
                    return *a < *b ? -1 : 1;
            continue;
        }
        if (Fold(*a) != Fold(*b))
            return Fold(*a) < Fold(*b) ? -1 : 1;
        a++;
        b++;
    }
    if (*a || *b)
        return *a ? 1 : -1; // the shorter one is a prefix of the other
    return 0;
}

} // namespace natural
