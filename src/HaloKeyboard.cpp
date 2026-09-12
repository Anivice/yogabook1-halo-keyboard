#include "HaloKeyboard.h"

#include <algorithm>

#include "log.hpp"
#include "assert_throw.h"
#include "ExecuteCommands.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <linux/uinput.h>
#include <ranges>

void HaloKeyboard::worker()
{
    int next_id = 0;
    const auto touchpad_width = keyboard_layout_.at(KEY_ID_TOUCHPAD).key_pixel_bottom_right_x - keyboard_layout_.at(KEY_ID_TOUCHPAD).key_pixel_top_left_x;
    const auto touchpad_height = keyboard_layout_.at(KEY_ID_TOUCHPAD).key_pixel_bottom_right_y - keyboard_layout_.at(KEY_ID_TOUCHPAD).key_pixel_top_left_y;
    std::unordered_map < unsigned int /* slot */, key_id_t > slot_to_key_id_map;


    while (running_.load(std::memory_order_relaxed))
    {
        // wait until libinput_fd is ready
        if (poll(&pfd_, 1, 100) <= 0) {
            continue;
        }

        // tell libinput to process the pending data
        if (libinput_dispatch(li_) != 0) {
            continue;
        }

        libinput_event *ev;
        while ((ev = libinput_get_event(li_)))
        {
            if (const auto type = libinput_event_get_type(ev);
                type == LIBINPUT_EVENT_TOUCH_DOWN || type == LIBINPUT_EVENT_TOUCH_UP || type == LIBINPUT_EVENT_TOUCH_MOTION)
            {
                libinput_event_touch *tev = libinput_event_get_touch_event(ev);
                libinput_device *dev = libinput_event_get_device(ev);
                unsigned vendor = libinput_device_get_id_vendor(dev);
                unsigned product = libinput_device_get_id_product(dev);
                const char *name = libinput_device_get_name(dev);
                if (vendor != 1046 || product != 9110) { // FIXME: Lenovo Halo Keyboard ID (static, or variable for different models???)
                    print<is_error>("Device ", name, " (", vendor, ":", product, ") not recognized as Halo keyboard, ignored\n");
                    continue; // skipped the device
                }

                const auto slot = libinput_event_touch_get_seat_slot(tev);
                double x = 0.00f, y = 0.00f;
                if (type != LIBINPUT_EVENT_TOUCH_UP) {
                    x = libinput_event_touch_get_x_transformed(tev, 1920);
                    y = libinput_event_touch_get_y_transformed(tev, 2400);
                }

                key_id_t determined_key = KEY_ID_INVALID_KEY_CODE;
                // determine the key (fucking just iterate through) FIXME: NOT GOOD
                for (const auto & [key, location] : keyboard_layout_)
                {
                    if (is_this_within_key_location(x, y, location)) {
                        determined_key = key;
                        break;
                    }
                }

                if (determined_key != KEY_ID_INVALID_KEY_CODE || type == LIBINPUT_EVENT_TOUCH_UP)
                {
                    // mouse event
                    if ((determined_key == KEY_ID_MOUSELEFT
                         || determined_key == KEY_ID_MOUSERIGHT
                         || determined_key == KEY_ID_TOUCHPAD)
                        || (type == LIBINPUT_EVENT_TOUCH_UP && slot_to_key_id_map.contains(slot)
                            && (slot_to_key_id_map.at(slot) == KEY_ID_MOUSELEFT
                                || slot_to_key_id_map.at(slot) == KEY_ID_MOUSERIGHT
                                || slot_to_key_id_map.at(slot) == KEY_ID_TOUCHPAD)
                        ))
                    {
                        if (type == LIBINPUT_EVENT_TOUCH_UP) {
                            if (slot_to_key_id_map.contains(slot)) slot_to_key_id_map.erase(slot);
                        } else {
                            slot_to_key_id_map.emplace(slot, static_cast<key_id_t>(determined_key));
                        }
                        EmitKeys::touchpad_mouse_handler(keyboard_layout_, x, y, determined_key, mouse_fd_, type, slot, touchpad_width, touchpad_height, next_id);
                    }
                    // key release
                    else if (type == LIBINPUT_EVENT_TOUCH_UP)
                    {
                        if (const auto key_id = slot_to_key_id_map.contains(slot) ? slot_to_key_id_map.at(slot) : -1; key_id != -1)
                        {
                            if (slot_to_key_id_map.contains(slot)) slot_to_key_id_map.erase(slot);
                            print("Key ", key_id_translate(static_cast<key_id_t>(key_id)), " (", key_id, ") release registered, slot=", slot, "\n");
                            emit_keys_->pop_notifier_.push(key_id);
                        }
                    }
                    // key press
                    else if (type == LIBINPUT_EVENT_TOUCH_DOWN)
                    {
                        slot_to_key_id_map[slot] = determined_key;
                        print("Key ", key_id_translate(determined_key),
                              " (", determined_key, ") press registered, slot=", slot, ", coordinate=(", x, ", ", y, ")\n");
                        emit_keys_->push_notifier_.push(determined_key);
                        if (!haptic_command_.empty())
                        {
                            haptic_threads_.emplace_back([this] {
                                print("Haptic event triggering /bin/sh -c '", haptic_command_, "' ...");
                                auto copy_haptic_command_ = haptic_command_;
                                exec_command("/bin/sh", "", "-c", copy_haptic_command_);
                                print("...done.\n");
                            });

                            if (haptic_threads_.size() > 4096)
                            {
                                std::ranges::for_each(haptic_threads_, [](std::thread &t) {
                                    if (t.joinable()) t.join();
                                });

                                haptic_threads_.clear();
                            }
                        }
                    }
                }
                else {
                    print("Key pressed but no key associated with this location in key map. "
                          "axisCoordinates=(1920x2400, ", x, ", ", y, ")\n");
                }
            }

            libinput_event_destroy(ev);
        }
    }
}

namespace
{
    int open_restricted(const char *path, const int flags, void *) {
        return open(path, flags);
    }

    void close_restricted(const int fd, void *) {
        close(fd);
    }

    constexpr libinput_interface interface =
    {
        .open_restricted = open_restricted,
        .close_restricted = close_restricted,
    };

    int init_linux_input(const kbd_map & key_map)
    {
        const int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        assert_throw(fd > 0);

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
        assert_throw(ioctl(fd, UI_SET_KEYBIT, INVERTED_KEY_FNLOCK) != -1);
        assert_throw(ioctl(fd, UI_SET_KEYBIT, INVERTED_KEY_MUTE) != -1);
        assert_throw(ioctl(fd, UI_SET_KEYBIT, INVERTED_KEY_VOLUMEDOWN) != -1);
        assert_throw(ioctl(fd, UI_SET_KEYBIT, INVERTED_KEY_VOLUMEUP) != -1);
        assert_throw(ioctl(fd, UI_SET_KEYBIT, INVERTED_KEY_BRIGHTNESSDOWN) != -1);
        assert_throw(ioctl(fd, UI_SET_KEYBIT, INVERTED_KEY_BRIGHTNESSUP) != -1);
        assert_throw(ioctl(fd, UI_SET_KEYBIT, INVERTED_KEY_SEARCH) != -1);
        assert_throw(ioctl(fd, UI_SET_KEYBIT, INVERTED_KEY_PREVIOUSSONG) != -1);
        assert_throw(ioctl(fd, UI_SET_KEYBIT, INVERTED_KEY_PLAYPAUSE) != -1);
        assert_throw(ioctl(fd, UI_SET_KEYBIT, INVERTED_KEY_NEXTSONG) != -1);
        assert_throw(ioctl(fd, UI_SET_KEYBIT, INVERTED_KEY_PRINT) != -1);
        assert_throw(ioctl(fd, UI_SET_KEYBIT, INVERTED_KEY_AIRPLANEMODE) != -1);
        assert_throw(ioctl(fd, UI_SET_KEYBIT, INVERTED_KEY_SETTINGS) != -1);

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
        const int ufd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        assert_throw(ufd > 0);

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
        assert_throw(ioctl(ufd, UI_DEV_CREATE) != -1);
        return ufd;
    }
}

HaloKeyboard::HaloKeyboard(const std::string &key_map, std::string haptic_command) : haptic_command_(std::move(haptic_command))
{
    // Load keyboard layout
    print("Loading keymap...");
    std::ifstream ifs(key_map);
    assert_throw(ifs.is_open());
    keyboard_layout_ = read_key_map(ifs);
    ifs.close();
    print("done.\n");

    // init linux input
    print("Initializing Linux input interface for virtual keyboard...");
    vkbd_fd_ = init_linux_input(keyboard_layout_);
    print("done.\n");

    print("Initializing Linux input interface for virtual mouse...");
    mouse_fd_ = init_linux_mouse_input();
    print("done.\n");

    // load keyboard touchpad
    print("Initializing Halo keyboard input interface...\n");

    // 1. create libinput context
    print("    Creating udev and libinput context...\n");
    udev_ = udev_new();
    li_ = libinput_udev_create_context(&interface, nullptr, udev_);
    assert_throw(li_);

    // 2. assign seat
    print("    Assigning seat to seat0...\n");
    assert_throw(libinput_udev_assign_seat(li_, "seat0") == 0);
    print("    Export file descriptor...\n");
    halo_device_fd_ = libinput_get_fd(li_);
    print("    => File descriptor for device input is ", halo_device_fd_, "\n");
    print("done.\n");

    pfd_ = {
        .fd = halo_device_fd_,
        .events = POLLIN,
        .revents = 0
    };

    print("Main loop started, end handler by sending SIGINT(2) to current process (pid=", getpid(), ").\n");

    emit_keys_ = std::make_unique<EmitKeys>(vkbd_fd_);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    thread_ = std::thread(&HaloKeyboard::worker, this);
}

HaloKeyboard::~HaloKeyboard()
{
    print("Main loop stopping...\n");
    running_.store(false, std::memory_order_relaxed);
    for (auto & T : haptic_threads_) if (T.joinable()) T.join();
    if (thread_.joinable()) thread_.join();
    // delete devices
    libinput_unref(li_);
    udev_unref(udev_);

    close(halo_device_fd_);
    close(vkbd_fd_);
    close(mouse_fd_);
}
