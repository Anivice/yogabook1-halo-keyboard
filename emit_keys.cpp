#include <unistd.h>
#include <iostream>
#include <linux/uinput.h>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include "map_reader.h"
#include <ranges>
#include "emit_keys.h"

void emit(const int fd, const uint16_t type, const uint16_t code, const int32_t value)
{
    input_event ev{};
    ev.type = type;
    ev.code = code;
    ev.value = value;
    gettimeofday(&ev.time, nullptr);
    assert_throw(write(fd, &ev, sizeof(ev)) == sizeof(ev));
}

int init_linux_input(const kbd_map & key_map)
{
    const int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        throw std::runtime_error("Unable to initialize linux input");
    }

    /* 1.  Announce we will send EV_KEY events for  */
    assert_throw(ioctl(fd, UI_SET_EVBIT, EV_KEY) != -1);
    for (const auto & key : key_map | std::views::keys) {
        if (key != 512 && key != BTN_LEFT && key != BTN_RIGHT) { // exclude three mouse modifiers
            assert_throw(ioctl(fd, UI_SET_KEYBIT, key) != -1);
        }
    }

    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_HOME) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_END) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_SCROLLUP) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_SCROLLDOWN) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_FN) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_MUTE) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_VOLUMEDOWN) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_VOLUMEUP) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_BRIGHTNESSDOWN) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_BRIGHTNESSUP) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_SEARCH) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_PREVIOUSSONG) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_PLAYPAUSE) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_NEXTSONG) != -1);
    assert_throw(ioctl(fd, UI_SET_KEYBIT, KEY_PRINT) != -1);

    /* 2.  Create the virtual device */
    uinput_setup usetup{};
    std::strcpy(usetup.name, "Halo Keyboard");
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor  = 0x91cb;
    usetup.id.product = 0x91cb;
    usetup.id.version = 0x1;
    assert_throw(ioctl(fd, UI_DEV_SETUP, &usetup) != -1);
    assert_throw(ioctl(fd, UI_DEV_CREATE) != -1);
    usleep(5000);
    return fd;
}

int init_linux_mouse_input()
{
    int ufd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (ufd < 0) throw std::runtime_error("open /dev/uinput failed");

    /* event & key capability bits */
    assert_throw(ioctl(ufd, UI_SET_EVBIT, EV_KEY) != -1);
    assert_throw(ioctl(ufd, UI_SET_EVBIT, EV_ABS) != -1);
    assert_throw(ioctl(ufd, UI_SET_EVBIT, EV_SYN) != -1);

    assert_throw(ioctl(ufd, UI_SET_KEYBIT, BTN_LEFT) != -1);
    assert_throw(ioctl(ufd, UI_SET_KEYBIT, BTN_RIGHT) != -1);
    assert_throw(ioctl(ufd, UI_SET_KEYBIT, BTN_TOUCH) != -1);          /* REQUIRED */
    assert_throw(ioctl(ufd, UI_SET_KEYBIT, BTN_TOOL_FINGER) != -1);    /* REQUIRED */
    assert_throw(ioctl(ufd, UI_SET_KEYBIT, BTN_TOOL_DOUBLETAP) != -1); // 2-finger
    assert_throw(ioctl(ufd, UI_SET_KEYBIT, BTN_TOOL_TRIPLETAP) != -1); // 3-finger, required by some gestures in desktop
    // assert_throw(ioctl(ufd, UI_SET_KEYBIT, BTN_TOOL_QUADTAP) != -1);   // 4 finger

    assert_throw(ioctl(ufd, UI_SET_ABSBIT, ABS_MT_SLOT) != -1);
    assert_throw(ioctl(ufd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID) != -1);
    assert_throw(ioctl(ufd, UI_SET_ABSBIT, ABS_MT_POSITION_X) != -1);
    assert_throw(ioctl(ufd, UI_SET_ABSBIT, ABS_MT_POSITION_Y) != -1);
    assert_throw(ioctl(ufd, UI_SET_ABSBIT, ABS_MT_PRESSURE) != -1);
    assert_throw(ioctl(ufd, UI_SET_ABSBIT, ABS_X) != -1);
    assert_throw(ioctl(ufd, UI_SET_ABSBIT, ABS_Y) != -1);

    assert_throw(ioctl(ufd, UI_SET_PROPBIT, INPUT_PROP_POINTER) != -1); /* identify as touchpad */

    /* device ID/name */
    struct uinput_setup us = {};
    us.id.bustype = BUS_USB;
    us.id.vendor  = 0x91cb;
    us.id.product = 0x91cb;
    strcpy(us.name, "Halo TouchPad");
    assert_throw(ioctl(ufd, UI_DEV_SETUP, &us) != -1);

    uinput_abs_setup abs = { };

    /* ----- multitouch slot bookkeeping ----- */
    abs.code = ABS_MT_SLOT;
    abs.absinfo.minimum = 0;
    abs.absinfo.maximum = 9;          /* advertise 10 fingers */
    abs.absinfo.value   = -1; // avoid slot == 0 causing mask
    assert_throw(ioctl(ufd, UI_ABS_SETUP, &abs) != -1);
    std::memset(&abs, 0, sizeof(abs));

    abs.code = ABS_MT_TRACKING_ID;
    abs.absinfo.minimum = 0;
    abs.absinfo.maximum = 65535;
    assert_throw(ioctl(ufd, UI_ABS_SETUP, &abs) != -1);

    /* ----- X/Y ranges you already had ----- */
    abs.code = ABS_MT_POSITION_X; abs.absinfo.minimum = 0; abs.absinfo.maximum = 1279;
    assert_throw(ioctl(ufd, UI_ABS_SETUP, &abs) != -1);
    abs.code = ABS_MT_POSITION_Y; abs.absinfo.maximum = 799;
    assert_throw(ioctl(ufd, UI_ABS_SETUP, &abs) != -1);
    abs.code = ABS_X;  abs.absinfo.maximum = 1279; assert_throw(ioctl(ufd, UI_ABS_SETUP, &abs) != -1);
    abs.code = ABS_Y;  abs.absinfo.maximum = 799;  assert_throw(ioctl(ufd, UI_ABS_SETUP, &abs) != -1);

    /* ----- pressure (safe 0-255 span) ----- */
    abs.code = ABS_MT_PRESSURE; abs.absinfo.minimum = 0; abs.absinfo.maximum = 255;
    assert_throw(ioctl(ufd, UI_ABS_SETUP, &abs) != -1);
    abs.code = ABS_PRESSURE; assert_throw(ioctl(ufd, UI_ABS_SETUP, &abs) != -1);

    if (ioctl(ufd, UI_DEV_CREATE) == -1)
        throw std::runtime_error("UI_DEV_CREATE failed");

    return ufd;
}
