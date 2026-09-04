#ifndef KEY_ID_H_
#define KEY_ID_H_

#include <linux/input-event-codes.h>
#include <limits>

// Valid keycodes
enum key_id_t : unsigned int
{
     KEY_ID_INVALID_KEY_CODE = std::numeric_limits<unsigned int>::max(),

     // group 1
     KEY_ID_FN    = KEY_FN,
     KEY_ID_LCTRL = KEY_LEFTCTRL,
     KEY_ID_WIN   = KEY_LEFTMETA,
     KEY_ID_LALT  = KEY_LEFTALT,
     KEY_ID_SPACE = KEY_SPACE,
     KEY_ID_RALT  = KEY_RIGHTALT,
     KEY_ID_RCTRL = KEY_RIGHTCTRL,
     KEY_ID_PGUP  = KEY_PAGEUP,
     KEY_ID_UP    = KEY_UP,
     KEY_ID_PGDN  = KEY_PAGEDOWN,
     KEY_ID_LEFT  = KEY_LEFT,
     KEY_ID_DOWN  = KEY_DOWN,
     KEY_ID_RIGHT = KEY_RIGHT,

     // group 2
     KEY_ID_LSHIFT    = KEY_LEFTSHIFT,
     KEY_ID_Z         = KEY_Z,
     KEY_ID_X         = KEY_X, // 45
     KEY_ID_C         = KEY_C, // 46
     KEY_ID_V         = KEY_V, // 47
     KEY_ID_B         = KEY_B, // 48
     KEY_ID_N         = KEY_N, // 49
     KEY_ID_M         = KEY_M, // 50
     KEY_ID_LESS      = KEY_COMMA,
     KEY_ID_LARGER    = KEY_DOT,
     KEY_ID_QUESTION  = KEY_SLASH,
     KEY_ID_RSHIFT    = KEY_RIGHTSHIFT, // 54

     // group 3
     KEY_ID_CAPSLOCK      = KEY_CAPSLOCK,
     KEY_ID_A             = KEY_A, // 30
     KEY_ID_S             = KEY_S, // 31
     KEY_ID_D             = KEY_D, // 32
     KEY_ID_F             = KEY_F, // 33
     KEY_ID_G             = KEY_G, // 34
     KEY_ID_H             = KEY_H, // 35
     KEY_ID_J             = KEY_J, // 36
     KEY_ID_K             = KEY_K, // 37
     KEY_ID_L             = KEY_L, // 38
     KEY_ID_SEMICOLON     = KEY_SEMICOLON, // 39
     KEY_ID_DOUBLEQUOTE   = KEY_APOSTROPHE, // 40
     KEY_ID_ENTER         = KEY_ENTER, // 28

     // group 4
     KEY_ID_TAB        = KEY_TAB, // 15
     KEY_ID_Q          = KEY_Q, // 16
     KEY_ID_W          = KEY_W, // 17
     KEY_ID_E          = KEY_E, // 18
     KEY_ID_R          = KEY_R, // 19
     KEY_ID_T          = KEY_T, // 20
     KEY_ID_Y          = KEY_Y, // 21
     KEY_ID_U          = KEY_U, // 22
     KEY_ID_I          = KEY_I, // 23
     KEY_ID_O          = KEY_O, // 24
     KEY_ID_P          = KEY_P, // 25
     KEY_ID_LEFTBRACE  = KEY_LEFTBRACE, // 26
     KEY_ID_RIGHTBRACE = KEY_RIGHTBRACE, // 27
     KEY_ID_BACKSLASH  = KEY_BACKSLASH, // 43

     // group 5
     KEY_ID_GRAVE     = KEY_GRAVE, // 41
     KEY_ID_1         = KEY_1, // 2
     KEY_ID_2         = KEY_2, // 3
     KEY_ID_3         = KEY_3, // 4
     KEY_ID_4         = KEY_4, // 5
     KEY_ID_5         = KEY_5, // 6
     KEY_ID_6         = KEY_6, // 7
     KEY_ID_7         = KEY_7, // 8
     KEY_ID_8         = KEY_8, // 9
     KEY_ID_9         = KEY_9, // 10
     KEY_ID_0         = KEY_0, // 11
     KEY_ID_MINUS     = KEY_MINUS, // 12
     KEY_ID_EQUAL     = KEY_EQUAL, // 13
     KEY_ID_BACKSPACE = KEY_BACKSPACE, // 14

     // group 6
     KEY_ID_ESC       = KEY_ESC, // 1
     KEY_ID_F1        = KEY_F1, // 59
     KEY_ID_F2        = KEY_F2, // 60
     KEY_ID_F3        = KEY_F3, // 61
     KEY_ID_F4        = KEY_F4, // 62
     KEY_ID_F5        = KEY_F5, // 63
     KEY_ID_F6        = KEY_F6, // 64
     KEY_ID_F7        = KEY_F7, // 65
     KEY_ID_F8        = KEY_F8, // 66
     KEY_ID_F9        = KEY_F9, // 67
     KEY_ID_F10       = KEY_F10, // 68
     KEY_ID_F11       = KEY_F11, // 87
     KEY_ID_F12       = KEY_F12, // 88
     KEY_ID_DELETE    = KEY_DELETE, // 111

     // group 7
     KEY_ID_MOUSELEFT  = BTN_LEFT,
     KEY_ID_MOUSERIGHT = BTN_RIGHT,
     KEY_ID_TOUCHPAD   = 512,

     // inverted keys
     INVERTED_KEY_FNLOCK         = KEY_FN,
     INVERTED_KEY_MUTE           = KEY_MUTE,
     INVERTED_KEY_VOLUMEDOWN     = KEY_VOLUMEDOWN,
     INVERTED_KEY_VOLUMEUP       = KEY_VOLUMEUP,
     INVERTED_KEY_AIRPLANEMODE   = 0xFFFF02,
     INVERTED_KEY_BRIGHTNESSDOWN = KEY_BRIGHTNESSDOWN,
     INVERTED_KEY_BRIGHTNESSUP   = KEY_BRIGHTNESSUP,
     INVERTED_KEY_SEARCH         = KEY_SEARCH,
     INVERTED_KEY_SETTINGS       = 0xFFFF01,
     INVERTED_KEY_PREVIOUSSONG   = KEY_PREVIOUSSONG,
     INVERTED_KEY_PLAYPAUSE      = KEY_PLAYPAUSE,
     INVERTED_KEY_NEXTSONG       = KEY_NEXTSONG,
     INVERTED_KEY_PRINT          = KEY_PRINT,
};

/// This is the inverted key code (Fn inversion/switch) from F1 to F12
constexpr unsigned int F1_to_F12_list[] =
{
    INVERTED_KEY_MUTE,
    INVERTED_KEY_VOLUMEDOWN,
    INVERTED_KEY_VOLUMEUP,
    INVERTED_KEY_AIRPLANEMODE,
    INVERTED_KEY_BRIGHTNESSDOWN,
    INVERTED_KEY_BRIGHTNESSUP,
    INVERTED_KEY_SEARCH,
    INVERTED_KEY_SETTINGS,
    INVERTED_KEY_PREVIOUSSONG,
    INVERTED_KEY_PLAYPAUSE,
    INVERTED_KEY_NEXTSONG,
    INVERTED_KEY_PRINT,
};

#endif //KEY_ID_H_