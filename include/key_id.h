#ifndef KEY_ID_H_
#define KEY_ID_H_

// group 1
constexpr unsigned int KEY_ID_FN    = 464;
constexpr unsigned int KEY_ID_LCTRL = 29;
constexpr unsigned int KEY_ID_WIN   = 125;
constexpr unsigned int KEY_ID_LALT  = 56;
constexpr unsigned int KEY_ID_SPACE = 57;
constexpr unsigned int KEY_ID_RALT  = 100;
constexpr unsigned int KEY_ID_RCTRL = 97;
constexpr unsigned int KEY_ID_PGUP  = 104;
constexpr unsigned int KEY_ID_UP    = 103;
constexpr unsigned int KEY_ID_PGDN  = 109;
constexpr unsigned int KEY_ID_LEFT  = 105;
constexpr unsigned int KEY_ID_DOWN  = 108;
constexpr unsigned int KEY_ID_RIGHT = 106;

// group 2
constexpr unsigned int KEY_ID_LSHIFT    = 42;
constexpr unsigned int KEY_ID_Z         = 44;
constexpr unsigned int KEY_ID_X         = 45;
constexpr unsigned int KEY_ID_C         = 46;
constexpr unsigned int KEY_ID_V         = 47;
constexpr unsigned int KEY_ID_B         = 48;
constexpr unsigned int KEY_ID_N         = 49;
constexpr unsigned int KEY_ID_M         = 50;
constexpr unsigned int KEY_ID_LESS      = 51;
constexpr unsigned int KEY_ID_LARGER    = 52;
constexpr unsigned int KEY_ID_QUESTION  = 53;
constexpr unsigned int KEY_ID_RSHIFT    = 54;

// group 3
constexpr unsigned int KEY_ID_CAPSLOCK      = 58;
constexpr unsigned int KEY_ID_A             = 30;
constexpr unsigned int KEY_ID_S             = 31;
constexpr unsigned int KEY_ID_D             = 32;
constexpr unsigned int KEY_ID_F             = 33;
constexpr unsigned int KEY_ID_G             = 34;
constexpr unsigned int KEY_ID_H             = 35;
constexpr unsigned int KEY_ID_J             = 36;
constexpr unsigned int KEY_ID_K             = 37;
constexpr unsigned int KEY_ID_L             = 38;
constexpr unsigned int KEY_ID_SEMICOLON     = 39;
constexpr unsigned int KEY_ID_DOUBLEQUOTE   = 40;
constexpr unsigned int KEY_ID_ENTER         = 28;

// group 4
constexpr unsigned int KEY_ID_TAB        = 15;
constexpr unsigned int KEY_ID_Q          = 16;
constexpr unsigned int KEY_ID_W          = 17;
constexpr unsigned int KEY_ID_E          = 18;
constexpr unsigned int KEY_ID_R          = 19;
constexpr unsigned int KEY_ID_T          = 20;
constexpr unsigned int KEY_ID_Y          = 21;
constexpr unsigned int KEY_ID_U          = 22;
constexpr unsigned int KEY_ID_I          = 23;
constexpr unsigned int KEY_ID_O          = 24;
constexpr unsigned int KEY_ID_P          = 25;
constexpr unsigned int KEY_ID_LEFTBRACE  = 26;
constexpr unsigned int KEY_ID_RIGHTBRACE = 27;
constexpr unsigned int KEY_ID_BACKSLASH  = 43;

// group 5
constexpr unsigned int KEY_ID_GRAVE     = 41;
constexpr unsigned int KEY_ID_1         = 2;
constexpr unsigned int KEY_ID_2         = 3;
constexpr unsigned int KEY_ID_3         = 4;
constexpr unsigned int KEY_ID_4         = 5;
constexpr unsigned int KEY_ID_5         = 6;
constexpr unsigned int KEY_ID_6         = 7;
constexpr unsigned int KEY_ID_7         = 8;
constexpr unsigned int KEY_ID_8         = 9;
constexpr unsigned int KEY_ID_9         = 10;
constexpr unsigned int KEY_ID_0         = 11;
constexpr unsigned int KEY_ID_MINUS     = 12;
constexpr unsigned int KEY_ID_EQUAL     = 13;
constexpr unsigned int KEY_ID_BACKSPACE = 14;

// group 6
constexpr unsigned int KEY_ID_ESC       = 1;
constexpr unsigned int KEY_ID_F1        = 59;
constexpr unsigned int KEY_ID_F2        = 60;
constexpr unsigned int KEY_ID_F3        = 61;
constexpr unsigned int KEY_ID_F4        = 62;
constexpr unsigned int KEY_ID_F5        = 63;
constexpr unsigned int KEY_ID_F6        = 64;
constexpr unsigned int KEY_ID_F7        = 65;
constexpr unsigned int KEY_ID_F8        = 66;
constexpr unsigned int KEY_ID_F9        = 67;
constexpr unsigned int KEY_ID_F10       = 68;
constexpr unsigned int KEY_ID_F11       = 87;
constexpr unsigned int KEY_ID_F12       = 88;
constexpr unsigned int KEY_ID_DELETE    = 111;

// group 7
constexpr unsigned int KEY_ID_MOUSELEFT  = 272;
constexpr unsigned int KEY_ID_MOUSERIGHT = 273;
constexpr unsigned int KEY_ID_TOUCHPAD   = 512;

#include <map>

const std::map < const unsigned int, const std::string > key_id_to_str_translation_table =
{
    // group 1
    { KEY_ID_FN, "Fn" },
    { KEY_ID_LCTRL, "LCtrl" },
    { KEY_ID_WIN, "Win" },
    { KEY_ID_LALT, "LAlt" },
    { KEY_ID_SPACE, "Space" },
    { KEY_ID_RALT, "RAlt" },
    { KEY_ID_RCTRL, "RCtrl" },
    { KEY_ID_PGUP, "PgUp" },
    { KEY_ID_UP, "Up" },
    { KEY_ID_PGDN, "PgDn" },
    // + 3
    { KEY_ID_LEFT, "Left" },
    { KEY_ID_DOWN, "Down" },
    { KEY_ID_RIGHT, "Right" },

    // group 2
    { KEY_ID_LSHIFT, "LShift" },
    { KEY_ID_Z, "Z" },
    { KEY_ID_X, "X" },
    { KEY_ID_C, "C" },
    { KEY_ID_V, "V" },
    { KEY_ID_B, "B" },
    { KEY_ID_N, "N" },
    { KEY_ID_M, "M" },
    { KEY_ID_LESS, "<," },
    { KEY_ID_LARGER, ">." },
    { KEY_ID_QUESTION, "?/" },
    { KEY_ID_RSHIFT, "RShift" },

    // group 3
    { KEY_ID_CAPSLOCK, "CapsLock" },
    { KEY_ID_A, "A" },
    { KEY_ID_S, "S" },
    { KEY_ID_D, "D" },
    { KEY_ID_F, "F" },
    { KEY_ID_G, "G" },
    { KEY_ID_H, "H" },
    { KEY_ID_J, "J" },
    { KEY_ID_K, "K" },
    { KEY_ID_L, "L" },
    { KEY_ID_SEMICOLON, ":;" },
    { KEY_ID_DOUBLEQUOTE, "\"\'" },
    { KEY_ID_ENTER, "Enter" },

    // group 4
    { KEY_ID_TAB, "Tab" },
    { KEY_ID_Q, "Q" },
    { KEY_ID_W, "W" },
    { KEY_ID_E, "E" },
    { KEY_ID_R, "R" },
    { KEY_ID_T, "T" },
    { KEY_ID_Y, "Y" },
    { KEY_ID_U, "U" },
    { KEY_ID_I, "I" },
    { KEY_ID_O, "O" },
    { KEY_ID_P, "P" },
    { KEY_ID_LEFTBRACE, "{[" },
    { KEY_ID_RIGHTBRACE, "}]" },
    { KEY_ID_BACKSLASH, "|\\" },

    // group 5
    { KEY_ID_GRAVE, "~`" },
    { KEY_ID_1, "1" },
    { KEY_ID_2, "2" },
    { KEY_ID_3, "3" },
    { KEY_ID_4, "4" },
    { KEY_ID_5, "5" },
    { KEY_ID_6, "6" },
    { KEY_ID_7, "7" },
    { KEY_ID_8, "8" },
    { KEY_ID_9, "9" },
    { KEY_ID_0, "0" },
    { KEY_ID_MINUS, "_-" },
    { KEY_ID_EQUAL, "+=" },
    { KEY_ID_BACKSPACE, "BackSpace" },

    // group 6
    { KEY_ID_ESC, "ESC" },
    { KEY_ID_F1, "F1" },
    { KEY_ID_F2, "F2" },
    { KEY_ID_F3, "F3" },
    { KEY_ID_F4, "F4" },
    { KEY_ID_F5, "F5" },
    { KEY_ID_F6, "F6" },
    { KEY_ID_F7, "F7" },
    { KEY_ID_F8, "F8" },
    { KEY_ID_F9, "F9" },
    { KEY_ID_F10, "F10" },
    { KEY_ID_F11, "F11" },
    { KEY_ID_F12, "F12" },
    { KEY_ID_DELETE, "Delete" },

    // group 7
    { KEY_ID_MOUSELEFT, "MouseLeft" },
    { KEY_ID_MOUSERIGHT, "MouseRight" },
    { KEY_ID_TOUCHPAD, "TouchPad" },
};

#endif //KEY_ID_H_
