#include <algorithm>
#include "log.hpp"
#include "map_reader.h"
#include "emit_keys.h"
#include <fcntl.h>
#include <linux/uinput.h>
#include <csignal>
#include <mutex>
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
#include <key_id.h>

enum KeyAction { KEY_COMBINATION_ONLY };

// these keys are locked keys, locked keys are not released until one non-lockable key press
// when forming a combination with normal keys, long press mode is disabled
const std::map < const unsigned int, const KeyAction > SpecialKeys =
{
    // { 464   /* Fn */,       KEY_COMBINATION_ONLY },
    { 29    /* LCtrl */,    KEY_COMBINATION_ONLY },
    //{ 125   /* Win */,      KEY_COMBINATION_ONLY },
    { 56    /* LAlt */,     KEY_COMBINATION_ONLY },
    { 100   /* RAlt */,     KEY_COMBINATION_ONLY },
    { 97    /* RCtrl */,    KEY_COMBINATION_ONLY },
    { 42    /* LShift */,   KEY_COMBINATION_ONLY },
    { 54    /* RShift */,   KEY_COMBINATION_ONLY },
};

const std::vector < unsigned int > keys_supporting_long_press = {
    KEY_ID_SPACE,
    KEY_ID_PGUP,
    KEY_ID_UP,
    KEY_ID_PGDN,
    KEY_ID_LEFT,
    KEY_ID_DOWN,
    KEY_ID_RIGHT,
    KEY_ID_Z,
    KEY_ID_X,
    KEY_ID_C,
    KEY_ID_V,
    KEY_ID_B,
    KEY_ID_N,
    KEY_ID_M,
    KEY_ID_LESS,
    KEY_ID_LARGER,
    KEY_ID_QUESTION,
    KEY_ID_A,
    KEY_ID_S,
    KEY_ID_D,
    KEY_ID_F,
    KEY_ID_G,
    KEY_ID_H,
    KEY_ID_J,
    KEY_ID_K,
    KEY_ID_L,
    KEY_ID_SEMICOLON,
    KEY_ID_DOUBLEQUOTE,
    KEY_ID_ENTER,
    KEY_ID_TAB,
    KEY_ID_Q,
    KEY_ID_W,
    KEY_ID_E,
    KEY_ID_R,
    KEY_ID_T,
    KEY_ID_Y,
    KEY_ID_U,
    KEY_ID_I,
    KEY_ID_O,
    KEY_ID_P,
    KEY_ID_LEFTBRACE,
    KEY_ID_RIGHTBRACE,
    KEY_ID_BACKSLASH,
    KEY_ID_GRAVE,
    KEY_ID_1,
    KEY_ID_2,
    KEY_ID_3,
    KEY_ID_4,
    KEY_ID_5,
    KEY_ID_6,
    KEY_ID_7,
    KEY_ID_8,
    KEY_ID_9,
    KEY_ID_0,
    KEY_ID_MINUS,
    KEY_ID_EQUAL,
    KEY_ID_BACKSPACE,
    KEY_ID_DELETE,
};

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

volatile std::atomic_int ctrl_c = 0;
volatile std::atomic_int halo_device_fd = -1;
using key_id_t = unsigned int;
struct key_state_t {
    bool normal_press_handled = false;
    bool press_down = false;
    std::chrono::time_point<std::chrono::high_resolution_clock> press_event_reg_time;
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
        return true;
    }

    return false;
}

void emit_key_thread()
{
    pthread_setname_np(pthread_self(), "VirKbd");

    struct long_press_struct {
        std::unique_ptr < std::atomic_bool > running;
        std::thread thread;
        key_id_t key{};
    };
    thread_local std::vector < long_press_struct > long_pressed_keys;

    std::vector < unsigned int > combination_sp_keys;
    std::mutex mutex_;

    auto press_keys_once = [&](const key_id_t pressed_key)->void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (DEBUG) {
            debug::log("Executing key press "
                "fn:", key_id_translate(combination_sp_keys),
                " & ch:", key_id_translate(pressed_key), "\n");
        }

        for (const auto key_id : combination_sp_keys) {
            emit(vkbd_fd, EV_KEY, key_id /* key code */, 1 /* press down */);
        }
        emit(vkbd_fd, EV_KEY, pressed_key /* key code */, 1 /* press down */);
        emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync

        for (const auto key_id : combination_sp_keys) {
            emit(vkbd_fd, EV_KEY, key_id /* key code */, 0 /* release */);
        }
        emit(vkbd_fd, EV_KEY, pressed_key /* key code */, 0 /* release */);
        emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync
    };

    while (!ctrl_c)
    {
        {
            std::vector<unsigned int> invalid_keys;
            std::lock_guard guard(g_mutex);

            for (auto & [key_id, state] : pressed_key)
            {
                ///////////////////////////////////
                /// KEY PRESS PROCESSING REGION ///
                ///////////////////////////////////
                if (!state.normal_press_handled)
                {
                    if (state.press_down && SpecialKeys.contains(key_id))
                    {
                        const auto sp_key_id = SpecialKeys.at(key_id);
                        if (sp_key_id == KEY_COMBINATION_ONLY)
                        {
                            combination_sp_keys.push_back(key_id);
                            if (DEBUG) debug::log("Functional key ", key_id_translate(key_id), " registered\n");
                            state.normal_press_handled = true;
                        }
                    }
                    /////////////////
                    /// PRESS KEY ///
                    /////////////////
                    else if (state.press_down)
                    {
                        press_keys_once(key_id);
                        state.normal_press_handled = true;
                    }
                    ///////////////////
                    /// RELEASE KEY ///
                    ///////////////////
                    else if(!state.press_down)
                    {
                        // remove key
                        // is the key being long pressed?
                        const auto it = std::ranges::find_if(long_pressed_keys,
                            [&](const long_press_struct & ins_state)->bool { return ins_state.key == key_id; });
                        if (it != long_pressed_keys.end()) { // found thread
                            *it->running = false;
                            if (it->thread.joinable()) {
                                it->thread.join();
                            }

                            // !! delete reference !!, it is now INVALID
                            std::erase_if(long_pressed_keys,
                                [&](const long_press_struct & ins_state)->bool { return ins_state.key == key_id; });
                        }

                        std::lock_guard<std::mutex> lock(mutex_);
                        std::erase(combination_sp_keys, key_id);
                        invalid_keys.push_back(key_id);
                        state.normal_press_handled = true;
                        if (DEBUG) debug::log("Key ", key_id_translate(key_id), " released\n");
                    }
                }

                ////////////////////////////////////
                /// LONG PRESS PROCESSING REGION ///
                ////////////////////////////////////
                if (const auto time_now = std::chrono::high_resolution_clock::now();
                    // is initial key press already handled and still being pressed down?
                    state.normal_press_handled && state.press_down
                    // does this key actually support long press?
                    && std::ranges::find(keys_supporting_long_press, key_id) != keys_supporting_long_press.end()
                    // no handler actively running for this key
                    && std::ranges::find_if(long_pressed_keys,
                        [&](const long_press_struct & ins_state)->bool { return ins_state.key == key_id; }) == long_pressed_keys.end()
                    // key pressed and escalated for more than 500ms?
                    && (time_now - state.press_event_reg_time) > std::chrono::milliseconds(500))
                {
                    auto long_press_event_handler = [&](const std::atomic_bool * should_i_be_running)->void
                    {
                        if (DEBUG) pthread_setname_np(pthread_self(), ("Long Press " + key_id_translate(key_id)).c_str());
                        else pthread_setname_np(pthread_self(), "Long Press");
                        while (*should_i_be_running)
                        {
                            press_keys_once(key_id);
                            std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        }
                    };

                    long_pressed_keys.emplace_back((long_press_struct){
                        .running = std::make_unique<std::atomic_bool>(true),
                        .key = key_id,
                    });

                    // create thread:
                    long_pressed_keys.back().thread = std::thread(long_press_event_handler,
                        long_pressed_keys.back().running.get());
                    if (DEBUG) debug::log("Created thread for long press key ", key_id_translate(key_id), "\n");
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

    std::map < unsigned int /* slot */, key_id_t > slot_to_key_id_map;

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
                    // mouse event
                    if ((determined_key == KEY_ID_MOUSELEFT || determined_key == KEY_ID_MOUSERIGHT || determined_key == KEY_ID_TOUCHPAD))
                    {
                        touchpad_mouse_handler(map, x, y, determined_key, mouse_fd, type, slot, touchpad_width, touchpad_height, next_id);
                    }
                    // key release
                    else if (type == LIBINPUT_EVENT_TOUCH_UP)
                    {
                        const auto key_id = slot_to_key_id_map.contains(slot) ? slot_to_key_id_map.at(slot) : -1;
                        if (DEBUG) {
                            debug::log("Key ", key_id_translate(key_id),
                                " (", key_id, ") "
                                "release registered, slot=", slot, "\n");
                        }

                        std::lock_guard<std::mutex> lock(g_mutex);
                        if (const auto it = pressed_key.find(key_id);
                            it != pressed_key.end())
                        {
                            it->second.normal_press_handled = false;
                            it->second.press_down = false;
                        }
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
                            pressed_key[determined_key].press_down = true;
                            pressed_key[determined_key].press_event_reg_time = time_of_the_last_press_event[determined_key];
                            slot_to_key_id_map[slot] = determined_key;
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
