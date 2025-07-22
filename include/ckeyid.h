#ifndef CKEYID_H
#define CKEYID_H

#include <linux/input-event-codes.h>

// group 1
#define KEY_ID_FN       ((unsigned int)464)
#define KEY_ID_LCTRL    ((unsigned int)29)
#define KEY_ID_WIN      ((unsigned int)125)
#define KEY_ID_LALT     ((unsigned int)56)
#define KEY_ID_SPACE    ((unsigned int)57)
#define KEY_ID_RALT     ((unsigned int)100)
#define KEY_ID_RCTRL    ((unsigned int)97)
#define KEY_ID_PGUP     ((unsigned int)104)
#define KEY_ID_UP       ((unsigned int)103)
#define KEY_ID_PGDN     ((unsigned int)109)
#define KEY_ID_LEFT     ((unsigned int)105)
#define KEY_ID_DOWN     ((unsigned int)108)
#define KEY_ID_RIGHT    ((unsigned int)106)

// group 2
#define KEY_ID_LSHIFT   ((unsigned int)42)
#define KEY_ID_Z        ((unsigned int)44)
#define KEY_ID_X        ((unsigned int)45)
#define KEY_ID_C        ((unsigned int)46)
#define KEY_ID_V        ((unsigned int)47)
#define KEY_ID_B        ((unsigned int)48)
#define KEY_ID_N        ((unsigned int)49)
#define KEY_ID_M        ((unsigned int)50)
#define KEY_ID_LESS     ((unsigned int)51)
#define KEY_ID_LARGER   ((unsigned int)52)
#define KEY_ID_QUESTION ((unsigned int)53)
#define KEY_ID_RSHIFT   ((unsigned int)54)

// group 3
#define KEY_ID_CAPSLOCK ((unsigned int)58)
#define KEY_ID_A        ((unsigned int)30)
#define KEY_ID_S        ((unsigned int)31)
#define KEY_ID_D        ((unsigned int)32)
#define KEY_ID_F        ((unsigned int)33)
#define KEY_ID_G        ((unsigned int)34)
#define KEY_ID_H        ((unsigned int)35)
#define KEY_ID_J        ((unsigned int)36)
#define KEY_ID_K        ((unsigned int)37)
#define KEY_ID_L        ((unsigned int)38)
#define KEY_ID_SEMICOLON    ((unsigned int)39)
#define KEY_ID_DOUBLEQUOTE  ((unsigned int)40)
#define KEY_ID_ENTER    ((unsigned int)28)

// group 4
#define KEY_ID_TAB      ((unsigned int)15)
#define KEY_ID_Q        ((unsigned int)16)
#define KEY_ID_W        ((unsigned int)17)
#define KEY_ID_E        ((unsigned int)18)
#define KEY_ID_R        ((unsigned int)19)
#define KEY_ID_T        ((unsigned int)20)
#define KEY_ID_Y        ((unsigned int)21)
#define KEY_ID_U        ((unsigned int)22)
#define KEY_ID_I        ((unsigned int)23)
#define KEY_ID_O        ((unsigned int)24)
#define KEY_ID_P        ((unsigned int)25)
#define KEY_ID_LEFTBRACE  ((unsigned int)26)
#define KEY_ID_RIGHTBRACE ((unsigned int)27)
#define KEY_ID_BACKSLASH  ((unsigned int)43)

// group 5
#define KEY_ID_GRAVE    ((unsigned int)41)
#define KEY_ID_1        ((unsigned int)2)
#define KEY_ID_2        ((unsigned int)3)
#define KEY_ID_3        ((unsigned int)4)
#define KEY_ID_4        ((unsigned int)5)
#define KEY_ID_5        ((unsigned int)6)
#define KEY_ID_6        ((unsigned int)7)
#define KEY_ID_7        ((unsigned int)8)
#define KEY_ID_8        ((unsigned int)9)
#define KEY_ID_9        ((unsigned int)10)
#define KEY_ID_0        ((unsigned int)11)
#define KEY_ID_MINUS    ((unsigned int)12)
#define KEY_ID_EQUAL    ((unsigned int)13)
#define KEY_ID_BACKSPACE ((unsigned int)14)

// group 6
#define KEY_ID_ESC      ((unsigned int)1)
#define KEY_ID_F1       ((unsigned int)59)
#define KEY_ID_F2       ((unsigned int)60)
#define KEY_ID_F3       ((unsigned int)61)
#define KEY_ID_F4       ((unsigned int)62)
#define KEY_ID_F5       ((unsigned int)63)
#define KEY_ID_F6       ((unsigned int)64)
#define KEY_ID_F7       ((unsigned int)65)
#define KEY_ID_F8       ((unsigned int)66)
#define KEY_ID_F9       ((unsigned int)67)
#define KEY_ID_F10      ((unsigned int)68)
#define KEY_ID_F11      ((unsigned int)87)
#define KEY_ID_F12      ((unsigned int)88)
#define KEY_ID_DELETE   ((unsigned int)111)

// group 7
#define KEY_ID_MOUSELEFT    ((unsigned int)272)
#define KEY_ID_MOUSERIGHT   ((unsigned int)273)
#define KEY_ID_TOUCHPAD     ((unsigned int)512)

enum fn_inverted_keys:unsigned int {
    INVERTED_KEY_FNLOCK = KEY_FN,
    INVERTED_KEY_MUTE = KEY_MUTE,
    INVERTED_KEY_VOLUMEDOWN = KEY_VOLUMEDOWN,
    INVERTED_KEY_VOLUMEUP = KEY_VOLUMEUP,
    INVERTED_KEY_AIRPLANEMODE = 0xFFFF02,
    INVERTED_KEY_BRIGHTNESSDOWN = KEY_BRIGHTNESSDOWN,
    INVERTED_KEY_BRIGHTNESSUP = KEY_BRIGHTNESSUP,
    INVERTED_KEY_SEARCH = KEY_SEARCH,
    INVERTED_KEY_SETTINGS = 0xFFFF01,
    INVERTED_KEY_PREVIOUSSONG = KEY_PREVIOUSSONG,
    INVERTED_KEY_PLAYPAUSE = KEY_PLAYPAUSE,
    INVERTED_KEY_NEXTSONG = KEY_NEXTSONG,
    INVERTED_KEY_PRINT = KEY_PRINT,
};

#endif //CKEYID_H
