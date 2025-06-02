#include <algorithm>
#include "log.hpp"
#include "map_reader.h"
#include "emit_keys.h"
#include <fcntl.h>
#include <linux/uinput.h>
#include <csignal>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <libinput.h>
#include <libudev.h>
#include <poll.h>
#include <iostream>
#include <ranges>
#include <functional>
#include <linux/kd.h>
#include <sys/ioctl.h>

constexpr unsigned int dispatch_repeated_key_delay_ms = 70;
constexpr unsigned int touchpad_mouse_region_id = 512;
enum KeyAction { RELEASE_ALL_KEYS, KEY_COMBINATION_ONLY, KEY_PERSISTS_UNTIL_FN_RELEASE };
const std::map < const unsigned int, const KeyAction > SpecialKeys =
{
    { 464   /* Fn */,       RELEASE_ALL_KEYS },
    { 29    /* LCtrl */,    KEY_COMBINATION_ONLY },
    { 125   /* Win */,      KEY_COMBINATION_ONLY },
    { 56    /* LAlt */,     KEY_COMBINATION_ONLY },
    { 100   /* RAlt */,     KEY_COMBINATION_ONLY },
    { 97    /* RCtrl */,    KEY_COMBINATION_ONLY },
    { 42    /* LShift */,   KEY_COMBINATION_ONLY },
    { 54    /* RShift */,   KEY_PERSISTS_UNTIL_FN_RELEASE },
};

const std::map < const unsigned int, const std::string > key_id_to_str_translation_table =
{
    // group 1
    {464, "Fn"},
    {29, "LCtrl"},
    {125, "Win"},
    {56, "LAlt"},
    {57, "Space"},
    {100, "RAlt"},
    {97, "RCtrl"},
    {104, "PgUp"},
    {103, "Up"},
    {109, "PgDn"},
    // + 3
    {105, "Left"},
    {108, "Down"},
    {106, "Right"},

    // group 2
    { 42, "LShift" },
    { 44, "Z" },
    { 45, "X" },
    { 46, "C" },
    { 47, "V" },
    { 48, "B" },
    { 49, "N" },
    { 50, "M" },
    { 51, "<," },
    { 52, ">." },
    { 53, "?/" },
    { 54, "RShift" },

    // group 3
    { 58, "CapsLock" },
    { 30, "A" },
    { 31, "S" },
    { 32, "D" },
    { 33, "F" },
    { 34, "G" },
    { 35, "H" },
    { 36, "J" },
    { 37, "K" },
    { 38, "L" },
    { 39, ":;" },
    { 40, "\"\'" },
    { 28, "Enter" },

    // group 4
    { 15, "Tab" },
    { 16, "Q" },
    { 17, "W" },
    { 18, "E" },
    { 19, "R" },
    { 20, "T" },
    { 21, "Y" },
    { 22, "U" },
    { 23, "I" },
    { 24, "O" },
    { 25, "P" },
    { 26, "{[" },
    { 27, "}]" },
    { 43, "|\\" },

    // group 5
    { 41, "~`" },
    { 2, "1" },
    { 3, "2" },
    { 4, "3" },
    { 5, "4" },
    { 6, "5" },
    { 7, "6" },
    { 8, "7" },
    { 9, "8" },
    { 10, "9" },
    { 11, "0" },
    { 12, "_-" },
    { 13, "+=" },
    { 14, "BackSpace" },

    // group 6
    { 1, "ESC" },
    { 59, "F1" },
    { 60, "F2" },
    { 61, "F3" },
    { 62, "F4" },
    { 63, "F5" },
    { 64, "F6" },
    { 65, "F7" },
    { 66, "F8" },
    { 67, "F9" },
    { 68, "F10" },
    { 87, "F11" },
    { 88, "F12" },
    { 111, "Delete" },

    // group 7
    { 272, "MouseLeft" },
    { 273, "MouseRight" },
    { touchpad_mouse_region_id, "TouchPad" },
};

volatile std::atomic_int ctrl_c = 0;
volatile std::atomic_int halo_device_fd = -1;
using key_id_t = unsigned int;
struct key_state_t {
    bool normal_press_handled = false;
};

std::mutex g_mutex;
std::map < key_id_t, key_state_t > pressed_key;
std::atomic_int vkbd_fd (-1);

void sigint_handler(int)
{
    debug::log("Stopping...\n");
    ctrl_c = 1;
    if (halo_device_fd != -1) {
        close(halo_device_fd);
    }
}

inline std::string key_id_translate(const key_id_t key)
{
    const auto it = key_id_to_str_translation_table.find(key);
    if (it == key_id_to_str_translation_table.end()) {
        return "Unknown";
    }

    return it->second;
}

template < typename Type > requires (!std::is_integral_v<Type>)
std::vector < std::string > key_id_translate(const Type & keys)
{
    std::vector < std::string > ret;
    ret.reserve(keys.size());
    for (const auto & key : keys) {
        static_assert(std::is_same_v < decltype(key), const key_id_t & >,
            "Decompressed element does not speaking the type `key_id_t`");
        ret.push_back(key_id_translate(key));
    }

    return ret;
}

bool if_capslock_enabled()
// NOTE: This programs works in system service and cannot query user-wise wayland compositor directly
// wayland sessions are per-session per-user, cross user resource referencing is strictly prohibited in many, if not all, compositors
// thus, the following query works only on active VT (Virtual Terminal, the actually text only environment), not with wayland enabled
// With wayland session active, user does not own '/dev/console' any longer and CapsLock will always be reported as Not Locked
{
    const int fd = open("/dev/console", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return false;
    }

    int leds;
    if (ioctl(fd, KDGETLED, &leds) == -1) {
        perror("ioctl");
        return false;
    }

    close(fd);

    if (leds & LED_CAP) { /* LED_CAP == 0x04 */
        // if (DEBUG) { debug::log("CapsLock reported as enabled\n"); }
        return true;
    }

    // if (DEBUG) { debug::log("CapsLock reported as disabled\n"); }
    return false;
}

void emit_key_thread()
{
    pthread_setname_np(pthread_self(), "VirKbd");
    thread_local std::vector < unsigned int > combination_sp_keys;
    thread_local std::vector < unsigned int > persistent_keys;

    while (!ctrl_c)
    {
        {
            std::vector<unsigned int> invalid_keys;
            std::lock_guard guard(g_mutex);

            for (auto & [key_id, state] : pressed_key)
            {
                if (!state.normal_press_handled)
                {
                    if (SpecialKeys.contains(key_id))
                    {
                        const auto sp_key_id = SpecialKeys.at(key_id);
                        if (sp_key_id == RELEASE_ALL_KEYS)
                        {
                            for (auto & _key : pressed_key | std::views::keys) {
                                emit(vkbd_fd, EV_KEY, _key /* key code */, 0 /* release key */);
                                emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync
                                invalid_keys.emplace_back(_key);
                                if (DEBUG) debug::log("Released key ", key_id_translate(_key), "\n");
                            }

                            for (auto & _key : persistent_keys) {
                                emit(vkbd_fd, EV_KEY, _key /* key code */, 0 /* release key */);
                                emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync
                                if (DEBUG) debug::log("Released key ", key_id_translate(_key), "\n");
                            }

                            // check if CapsLock enabled on VT
                            if (if_capslock_enabled())
                            {
                                emit(vkbd_fd, EV_KEY, KEY_CAPSLOCK, 1);
                                emit(vkbd_fd, EV_SYN, SYN_REPORT, 0);
                                emit(vkbd_fd, EV_KEY, KEY_CAPSLOCK, 0);
                                emit(vkbd_fd, EV_SYN, SYN_REPORT, 0);
                                if (DEBUG) debug::log("Released key ", key_id_translate(KEY_CAPSLOCK), "\n");
                            }

                            combination_sp_keys.clear();
                            persistent_keys.clear();
                            if (DEBUG) {
                                debug::log("All keys are now released\n");
                            }
                            break; // stop processing rest of the keys
                        }
                        else if (sp_key_id == KEY_COMBINATION_ONLY)
                        {
                            combination_sp_keys.push_back(key_id);
                            if (DEBUG) debug::log("Functional key ", key_id_translate(key_id), " registered\n");
                            state.normal_press_handled = true;
                        }
                        else if (sp_key_id == KEY_PERSISTS_UNTIL_FN_RELEASE)
                        {
                            auto element = std::ranges::find(persistent_keys, key_id);
                            if (element != std::ranges::end(persistent_keys))
                            {
                                emit(vkbd_fd, EV_KEY, *element /* key code */, 0 /* release key */);
                                emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync
                                persistent_keys.erase(element);
                                invalid_keys.emplace_back(key_id);
                                if (DEBUG) {
                                    debug::log("Persistent functional key ", key_id_translate(key_id), " released\n");
                                }
                            }
                            else
                            {
                                persistent_keys.push_back(key_id);
                                emit(vkbd_fd, EV_KEY, key_id /* key code */, 1 /* press down */);
                                emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync
                                state.normal_press_handled = true;
                                invalid_keys.push_back(key_id);
                                if (DEBUG) {
                                    debug::log("Persistent functional key ", key_id_translate(key_id), " registered\n");
                                }
                            }
                        }
                    }
                    else
                    {
                        if (DEBUG) {
                            debug::log("Executing key combination "
                                "fn:", key_id_translate(combination_sp_keys),
                                " & ch:", key_id_translate(key_id), "\n");
                        }

                        if (!combination_sp_keys.empty())
                        {
                            if (DEBUG) debug::log("Normal key combination handler\n");
                            // press combination
                            for (const auto key : combination_sp_keys) {
                                emit(vkbd_fd, EV_KEY, key /* key code */, 1 /* press down */);
                            }
                            emit(vkbd_fd, EV_KEY, key_id /* key code */, 1 /* press down */);
                            emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync

                            // release all keys
                            for (const auto key : combination_sp_keys) {
                                emit(vkbd_fd, EV_KEY, key /* key code */, 0);
                            }
                            emit(vkbd_fd, EV_KEY, key_id /* key code */, 0);
                            emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync

                            // clear all keys in the buffer
                            for (const auto &_key: combination_sp_keys) {
                                invalid_keys.push_back(_key);
                            }
                            invalid_keys.push_back(key_id);
                            combination_sp_keys.clear();
                            state.normal_press_handled = true;

                            break; // this will abort processing all pending keys inside the cache, until slots are deleted
                        } else {
                            if (DEBUG) {
                                debug::log("Normal key press ", key_id_translate(key_id),
                                    " (persisted: ", key_id_translate(persistent_keys),
                                    ", CapsLock=", if_capslock_enabled(), ")\n");
                            }
                            emit(vkbd_fd, EV_KEY, key_id /* key code */, 1 /* press down */);
                            emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync
                            emit(vkbd_fd, EV_KEY, key_id /* key code */, 0 /* release */);
                            emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync
                            state.normal_press_handled = true;
                            invalid_keys.push_back(key_id);
                        }
                    }
                }
            }

            for (const auto & inv_slot : invalid_keys) {
                pressed_key.erase(inv_slot);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    debug::log("Shutting down virtual keyboard...\n");
}

void touchpad_mouse_handler(const kbd_map & map,
    const double x, const double y,
    const unsigned int determined_key,
    const int mouse_fd,
    const libinput_event_type type,
    const int slot,
    const double touchpad_width, const double touchpad_height,
    int & next_id)
{
    if (type == LIBINPUT_EVENT_TOUCH_DOWN && (determined_key == BTN_LEFT || determined_key == BTN_RIGHT))
    {
        emit(mouse_fd, EV_ABS, ABS_MT_SLOT, slot);
        emit(mouse_fd, EV_KEY, determined_key, 1);
        emit(mouse_fd, EV_SYN, SYN_REPORT, 0); // sync
        emit(mouse_fd, EV_KEY, determined_key, 0);
        emit(mouse_fd, EV_SYN, SYN_REPORT, 0); // sync
        if (DEBUG) { debug::log("TouchPad ", key_id_translate(determined_key), " key pressed\n"); }
    }
    else if (determined_key == 512 || type == LIBINPUT_EVENT_TOUCH_UP)
    {
        const auto new_y = std::max(800 - static_cast<int>((x - map.at(512).key_pixel_top_left_x)
            / static_cast<double>(touchpad_width) * 800), 0);
        const auto new_x = std::max(static_cast<int>((y - map.at(512).key_pixel_top_left_y)
            / static_cast<double>(touchpad_height) * 1280), 0);

        /* 0. choose slot FIRST (always, even if it stays 0) */
        emit(mouse_fd, EV_ABS, ABS_MT_SLOT, slot);

        /* 1. contact bookkeeping */
        if (type == LIBINPUT_EVENT_TOUCH_DOWN) {
            emit(mouse_fd, EV_ABS, ABS_MT_TRACKING_ID, next_id++);
            emit(mouse_fd, EV_KEY, BTN_TOUCH,          1);
            emit(mouse_fd, EV_KEY, BTN_TOOL_FINGER,    1);
            emit(mouse_fd, EV_ABS, ABS_MT_PRESSURE, 128);
        } else if (type == LIBINPUT_EVENT_TOUCH_UP) {
            emit(mouse_fd, EV_ABS, ABS_MT_PRESSURE, 0);
            emit(mouse_fd, EV_ABS, ABS_MT_TRACKING_ID, -1);
            emit(mouse_fd, EV_KEY, BTN_TOUCH,          0);
            emit(mouse_fd, EV_KEY, BTN_TOOL_FINGER,    0);
        }

        /* 2. coordinates while finger is down */
        if (type != LIBINPUT_EVENT_TOUCH_UP) {
            emit(mouse_fd, EV_ABS, ABS_MT_POSITION_X, new_x);
            emit(mouse_fd, EV_ABS, ABS_MT_POSITION_Y, new_y);

            /* mirror slot-0 to single-touch axes */
            if (slot == 0) {
                emit(mouse_fd, EV_ABS, ABS_X, new_x);
                emit(mouse_fd, EV_ABS, ABS_Y, new_y);
            }
        }

        /* 3. flush the packet */
        emit(mouse_fd, EV_SYN, SYN_REPORT, 0);

        if (DEBUG) { debug::log("TouchPad movement (", x, ", ", y, ") "
            "mapped to (", new_x, ", ", new_y, "), slot=", slot, "\n"); }
    }
}

static int open_restricted(const char *path, int flags, void *user_data) {
    return open(path, flags);
}

static void close_restricted(int fd, void *) {
    close(fd);
}

static const libinput_interface interface =
{
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <map_file>" << std::endl;
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, sigint_handler);

    // read key map
    debug::log("Loading keymap...");
    std::ifstream ifs(argv[1]);
    if (!ifs.is_open()) {
        std::cerr << "Unable to open file" << std::endl;
        return EXIT_FAILURE;
    }
    const auto map = read_key_map(ifs);
    debug::log("done.\n");

    // init linux input
    debug::log("Initializing Linux input interface for virtual keyboard...");
    vkbd_fd = init_linux_input(map);
    debug::log("done.\n");

    debug::log("Initializing Linux input interface for virtual mouse...");
    int mouse_fd = init_linux_mouse_input();
    debug::log("done.\n");

    // load keyboard touchpad
    libinput *li = nullptr;
    debug::log("Initializing Halo keyboard input interface...");

    // 1. create libinput context
    if (DEBUG) debug::log("\n    Creating udev and libinput context...\n");
    udev *udev = udev_new();
    li = libinput_udev_create_context(&interface, nullptr, udev);
    if (!li) {
        std::cerr << "Failed to create libinput context\n";
        return EXIT_FAILURE;
    }

    // 2. assign seat
    if (DEBUG) debug::log("    Assigning seat to seat0...\n");
    assert_throw(libinput_udev_assign_seat(li, "seat0") == 0);
    if (DEBUG) debug::log("    Export file descriptor...\n");
    halo_device_fd = libinput_get_fd(li);
    if (DEBUG) debug::log("    => File descriptor for device input is ", halo_device_fd, "\n");
    debug::log("done.\n");

    // start virtual keyboard handling thread
    debug::log("Initializing virtual keyboard...");
    std::thread virtual_kbd_worker(emit_key_thread);
    debug::log("done.\n");

    pollfd pfd = { halo_device_fd, POLLIN, 0 };

    debug::log("Main loop started, end handler by sending SIGINT(2) to current process (pid=", getpid(), ").\n");

    std::map < key_id_t /* key */,
        std::chrono::time_point<std::chrono::high_resolution_clock>
    > time_of_the_last_press_event;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    int next_id = 0;
    const auto touchpad_width = map.at(512).key_pixel_bottom_right_x - map.at(512).key_pixel_top_left_x;
    const auto touchpad_height = map.at(512).key_pixel_bottom_right_y - map.at(512).key_pixel_top_left_y;

    while (!ctrl_c)
    {
        // wait until libinput_fd is ready
        if (poll(&pfd, 1, -1) <= 0) {
            continue;
        }

        // tell libinput to process the pending data
        assert_throw(libinput_dispatch(li) == 0);

        libinput_event *ev;
        while ((ev = libinput_get_event(li)))
        {
            const auto type = libinput_event_get_type(ev);
            if (type == LIBINPUT_EVENT_TOUCH_DOWN
                || type == LIBINPUT_EVENT_TOUCH_UP
                || type == LIBINPUT_EVENT_TOUCH_MOTION)
            {
                libinput_event_touch *tev =
                    libinput_event_get_touch_event(ev);

                libinput_device *dev = libinput_event_get_device(ev);
                unsigned vendor = libinput_device_get_id_vendor(dev);
                unsigned product = libinput_device_get_id_product(dev);
                const char *name = libinput_device_get_name(dev);
                if (vendor != 1046 || product != 9110) {
                    if (DEBUG) {
                        debug::log("Device ", name, " (", vendor, ":", product, ") not recognized as Halo keyboard, ignored\n");
                    }
                    continue; // skipped the loop
                }

                const int32_t slot = libinput_event_touch_get_seat_slot(tev);
                double x = 0.00f, y = 0.00f;
                if (type != LIBINPUT_EVENT_TOUCH_UP) {
                    x = libinput_event_touch_get_x_transformed(tev, 1920);
                    y = libinput_event_touch_get_y_transformed(tev, 2400);
                }

                long determined_key = -1;
                // determine the key (fucking just iterate through)
                for (const auto & [key, location] : map)
                {
                    if (is_this_within_key_location(x, y, location)) {
                        determined_key = key;
                        break;
                    }
                }

                if (determined_key != -1 || type == LIBINPUT_EVENT_TOUCH_UP)
                {
                    if ((map.contains(BTN_LEFT) && x <= map.at(BTN_LEFT).key_pixel_bottom_right_x && x > 0)
                        || type == LIBINPUT_EVENT_TOUCH_UP)
                    {
                        touchpad_mouse_handler(map, x, y, determined_key, mouse_fd, type, slot, touchpad_width, touchpad_height, next_id);
                    }
                    else if (type == LIBINPUT_EVENT_TOUCH_DOWN)
                    {
                        // keyboard only cares about `LIBINPUT_EVENT_TOUCH_DOWN`
                        if (time_of_the_last_press_event.contains(determined_key))
                        {
                            const auto now = std::chrono::high_resolution_clock::now();
                            const auto interval_since_press_down =
                                now - time_of_the_last_press_event.at(determined_key);
                            if (interval_since_press_down <
                                std::chrono::microseconds(50))
                            {
                                continue; // ignore consecutive key press
                            }
                        }

                        std::lock_guard guard(g_mutex);
                        // avoid conflicting keys
                        if (!pressed_key.contains(determined_key))
                        {
                            if (DEBUG) {
                                debug::log("Key ", key_id_translate(determined_key),
                                    " (", determined_key, ") "
                                    "press registered, slot=", slot, ", coordinate=(", x, ", ", y, ")\n");
                            }
                            time_of_the_last_press_event[determined_key] = std::chrono::high_resolution_clock::now();
                            pressed_key[determined_key].normal_press_handled = false;
                        }
                    }
                }
                else if (DEBUG) {
                    debug::log("Key pressed but no key associated with this location in key map. "
                        "axisCoordinates=(1920x2400, ", x, ", ", y, ")\n");
                }
                // Release is NOT processed. This is a typewriter and once key is pressed down, it is released automatically
            }

            libinput_event_destroy(ev);
        }
    }

    // delete devices
    libinput_unref(li);
    udev_unref(udev);

    if (virtual_kbd_worker.joinable()) {
        virtual_kbd_worker.join();
    }

    return EXIT_SUCCESS;
}
