#include <unistd.h>
#include <iostream>
#include <linux/uinput.h>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include "map_reader.h"
#include <ranges>
#include "emit_keys.h"
#include <unistd.h>

void emit(const int fd, const uint16_t type, const uint16_t code, const int32_t value)
{
    input_event ev{};
    ev.type = type;
    ev.code = code;
    ev.value = value;
    gettimeofday(&ev.time, nullptr);
    write(fd, &ev, sizeof(ev));
}

int init_linux_input(const kbd_map & key_map)
{
    const int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        throw std::runtime_error("Unable to initialize linux input");
    }

    /* 1.  Announce we will send EV_KEY events for, e.g., KEY_A */
    assert_throw(ioctl(fd, UI_SET_EVBIT, EV_KEY) != -1);
    for (const auto & key : key_map | std::views::keys) {
        // ioctl(fd, UI_SET_KEYBIT, KEY_A);
        assert_throw(ioctl(fd, UI_SET_KEYBIT, key) != -1);
    }

    /* 2.  Create the virtual device */
    uinput_setup usetup{};
    std::strcpy(usetup.name, "Halo Keyboard");
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor  = 0x91cb;
    usetup.id.product = 0x91cb;
    assert_throw(ioctl(fd, UI_DEV_SETUP, &usetup) != -1);
    assert_throw(ioctl(fd, UI_DEV_CREATE) != -1);
    usleep(5000);
    return fd;
}
