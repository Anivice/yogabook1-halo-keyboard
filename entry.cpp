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
#include <key_id.h>
#include "execute_command.h"
#include <filesystem>
#include "libmod.h"

constexpr unsigned int long_press_interval_ms = 80;
volatile std::atomic_int ctrl_c = 0;
volatile std::atomic_int halo_device_fd = -1;
namespace fs = std::filesystem;
using key_id_t = unsigned int;
struct key_state_t {
    bool normal_press_handled = false;
    bool press_down = false;
    std::chrono::time_point<std::chrono::high_resolution_clock> press_event_reg_time;
};

std::mutex g_mutex;
std::map < key_id_t, key_state_t > pressed_key;
std::atomic_int vkbd_fd (-1);
std::atomic_bool fnlock_enabled = false;

typedef void(*inverted_key_map_handler)(key_id_t);
void fn_lock(key_id_t);
extern "C" void normal_key_emit(key_id_t);
void settings(key_id_t);
void airplane_mode(key_id_t);

std::map < key_id_t, std::pair <key_id_t, inverted_key_map_handler> >
fn_key_invert_handler_map = {
    { KEY_ID_ESC, {INVERTED_KEY_FNLOCK,         fn_lock} },
    { KEY_ID_F1,  {INVERTED_KEY_MUTE,           normal_key_emit} },
    { KEY_ID_F2,  {INVERTED_KEY_VOLUMEDOWN,     normal_key_emit} },
    { KEY_ID_F3,  {INVERTED_KEY_VOLUMEUP,       normal_key_emit} },
    { KEY_ID_F4,  {INVERTED_KEY_AIRPLANEMODE,   airplane_mode} },
    { KEY_ID_F5,  {INVERTED_KEY_BRIGHTNESSDOWN, normal_key_emit} },
    { KEY_ID_F6,  {INVERTED_KEY_BRIGHTNESSUP,   normal_key_emit} },
    { KEY_ID_F7,  {INVERTED_KEY_SEARCH,         normal_key_emit} },
    { KEY_ID_F8,  {INVERTED_KEY_SETTINGS,       settings} },
    { KEY_ID_F9,  {INVERTED_KEY_PREVIOUSSONG,   normal_key_emit} },
    { KEY_ID_F10, {INVERTED_KEY_PLAYPAUSE,      normal_key_emit} },
    { KEY_ID_F11, {INVERTED_KEY_NEXTSONG,       normal_key_emit} },
    { KEY_ID_F12, {INVERTED_KEY_PRINT,          normal_key_emit} },
};

void sigint_handler(int)
{
    constexpr char output_message[] = { 'S', 't', 'o', 'p', 'p', 'i', 'n', 'g', '.', '.', '.', '\n' };
    write(1, output_message, sizeof(output_message));
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

void fn_lock(const key_id_t)
{
    fnlock_enabled = !fnlock_enabled;
    if (DEBUG) debug_log("Fn ", fnlock_enabled ? "" : "Un", "locked\n");
}

extern "C"
void normal_key_emit(const key_id_t key_id)
{
    emit(vkbd_fd, EV_KEY, key_id /* key code */, 1 /* press down */);
    emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync
    emit(vkbd_fd, EV_KEY, key_id /* key code */, 0 /* release */);
    emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync
}

std::vector<std::thread> xdg_thread_pool;

void settings(const key_id_t)
{
    if (DEBUG) debug_log("Launch System Settings\n");
    auto xdg_launch_settings = []()->void
    {
        exec_command("/usr/bin/env", "", "bash", "-c",
            "/usr/bin/machinectl shell "
            "--uid=1000 --setenv=XDG_RUNTIME_DIR=/run/user/1000 "
            "--setenv=WAYLAND_DISPLAY=wayland-0 "
            "--setenv=DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus "
            "--setenv=KDE_SESSION_VERSION=6 "
            "--setenv=KDE_FULL_SESSION=true \"$(id -un 1000)\"@ "
            "/usr/bin/systemsettings &");
    };

    xdg_thread_pool.emplace_back(xdg_launch_settings);
}

void airplane_mode(const key_id_t)
{
    if (DEBUG) debug_log("XDG setting Airplane Mode\n");
    debug_log("[WARNING] Toggling Airplane Mode not implemented and possibly never plan to. This key lacks practical purpose.\n");
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
    std::atomic_bool no_key_pressed_after_win = false;
    std::vector < unsigned int > combination_sp_keys;
    std::mutex mutex_;

    auto press_keys_once = [&](const key_id_t pressed_key)->void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (fn_key_invert_handler_map.contains(pressed_key))
        {
            if (DEBUG) debug_log("Fn governed key ", key_id_translate(pressed_key), " pressed, Fn is ",
                (std::ranges::find(combination_sp_keys, KEY_ID_FN) != combination_sp_keys.end() ? "" : "NOT "),
                "present within the key combination\n");
            // check if Fn Lock and Fn key presence within combinations
            bool do_i_invert = fnlock_enabled;
            do_i_invert = std::ranges::find(combination_sp_keys, KEY_ID_FN) != combination_sp_keys.end()
                ? !do_i_invert : do_i_invert;

            if (DEBUG) debug_log("Invert = ", do_i_invert, "\n");

            if (do_i_invert) {
                const auto [inverted_key_id, handler] = fn_key_invert_handler_map.at(pressed_key);
                if (DEBUG) debug_log("Pressing down ", key_id_translate(inverted_key_id), "\n");
                handler(inverted_key_id);
            } else { // just press the corresponding key
                if (DEBUG) debug_log("Pressing down ", key_id_translate(pressed_key), "\n");
                emit(vkbd_fd, EV_KEY, pressed_key /* key code */, 1 /* press down */);
                emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync
                emit(vkbd_fd, EV_KEY, pressed_key /* key code */, 0 /* release */);
                emit(vkbd_fd, EV_SYN, SYN_REPORT, 0); // sync
            }
        }
        else
        {
            if (DEBUG) {
                debug_log("Executing key press cb:", key_id_translate(combination_sp_keys),
                    " & ch:", key_id_translate(pressed_key), ", fn:", fnlock_enabled, "\n");
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
        }
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
                            if (DEBUG) debug_log("Functional key ", key_id_translate(key_id), " registered\n");
                            state.normal_press_handled = true;
                        }
                    }
                    /////////////////
                    /// PRESS KEY ///
                    /////////////////
                    else if (state.press_down)
                    {
                        if (key_id == KEY_ID_WIN) {
                            std::lock_guard local_guard(mutex_);
                            combination_sp_keys.push_back(KEY_ID_WIN);
                            no_key_pressed_after_win = true;
                        } else {
                            if (no_key_pressed_after_win) {
                                no_key_pressed_after_win = false;
                            }
                            press_keys_once(key_id);
                        }
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

                        if (key_id == KEY_ID_WIN && no_key_pressed_after_win) {
                            press_keys_once(KEY_ID_WIN); // now we press win
                            no_key_pressed_after_win = false;
                        }

                        std::lock_guard<std::mutex> lock(mutex_);
                        std::erase(combination_sp_keys, key_id); // this will delete WIN as well
                        invalid_keys.push_back(key_id);
                        state.normal_press_handled = true;
                        if (DEBUG) debug_log("Key ", key_id_translate(key_id), " released\n");
                    }
                }

                ////////////////////////////////////
                /// LONG PRESS PROCESSING REGION ///
                ////////////////////////////////////
                if (const auto time_now = std::chrono::high_resolution_clock::now();
                    // is the initial key press already handled and still being pressed down?
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
                        if constexpr (DEBUG) pthread_setname_np(pthread_self(), ("Long Press " + key_id_translate(key_id)).c_str());
                        else pthread_setname_np(pthread_self(), "Long Press");
                        while (*should_i_be_running)
                        {
                            press_keys_once(key_id);
                            std::this_thread::sleep_for(std::chrono::milliseconds(long_press_interval_ms));
                        }
                    };

                    long_pressed_keys.emplace_back((long_press_struct){
                        .running = std::make_unique<std::atomic_bool>(true),
                        .key = key_id,
                    });

                    // create thread:
                    long_pressed_keys.back().thread = std::thread(long_press_event_handler,
                        long_pressed_keys.back().running.get());
                    if (DEBUG) debug_log("Created thread for long press key ", key_id_translate(key_id), "\n");
                }
            }

            for (const auto & inv_slot : invalid_keys) {
                pressed_key.erase(inv_slot);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    debug_log("Shutting down virtual keyboard...");

    // shut down any unfinished threads
    for (auto & thread : long_pressed_keys)
    {
        *thread.running = false;
        if (thread.thread.joinable()) {
            thread.thread.join();
        }
    }

    for (auto & thread : xdg_thread_pool)
    {
        if (thread.joinable()) {
            thread.join();
        }
    }

    debug_log("done.\n");
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
        if (DEBUG) { debug_log("TouchPad ", key_id_translate(determined_key), " key pressed\n"); }
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

        if (DEBUG) { debug_log("TouchPad movement (", x, ", ", y, ") "
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
    debug_log("Halo Keyboard and TouchPad userspace driver [BuildID=", BUILD_ID, ", BuildTime=", BUILD_TIME, "]\n");
    bool fn_press = false;

    auto argv_3_parse = [&]()->void
    {
        if (const std::string key_press_init = argv[2]; key_press_init == "FN")
        {
            debug_log("FN registered as pressed\n");
            fn_press = true;
        }
        else if (key_press_init == "-") {}
        else
        {
            std::cout << "Cannot understand initialization verb \"" << key_press_init << "\"\n";
            exit(EXIT_FAILURE);
        }
    };

    std::unique_ptr <Module> fnmod;
    auto remap_fn_keys = [&]()->void
    {
        std::string mod_path = argv[3];
        fnmod = std::make_unique<Module>(mod_path);
        debug_log("Loaded Fn Key handler ", mod_path, "\n");
        for (auto & [key, handler] : fn_key_invert_handler_map | std::views::values)
        {
            if (key != INVERTED_KEY_FNLOCK)
            {
                handler = (inverted_key_map_handler)dlsym(fnmod->get_handler(), "fn_key_handler_vector");
                assert_throw(handler != nullptr);
            }
        }
    };

    if (argc == 3)
    {
        argv_3_parse();
    }
    else if (argc == 4)
    {
        argv_3_parse();
        remap_fn_keys();
    }
    else if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <map_file> [CAPS[,FN]]" << std::endl;
        return EXIT_FAILURE;
    }

    debug_log("Creating lock file...");
    const fs::path LockFilePath = "/tmp/.HaloKeyboard.lock";
    if (fs::exists(LockFilePath)) {
        std::cerr << "\n[ERROR] Lock file exists. If you believe this is an error, remove the file /tmp/.HaloKeyboard.lock\n";
        return EXIT_FAILURE;
    }
    else
    {
        std::ofstream ofs(LockFilePath);
        if (!ofs) {
            std::cerr << "Failed to create lock file\n";
            return EXIT_FAILURE;
        }
        ofs.close();
        debug_log("done.\n");
    }

    std::signal(SIGINT, sigint_handler);

    debug_log("Loading keymap...", '\n');

    // read key map
    debug_log("Loading keymap...");
    std::ifstream ifs(argv[1]);
    if (!ifs.is_open()) {
        std::cerr << "Unable to open file" << std::endl;
        return EXIT_FAILURE;
    }
    const auto map = read_key_map(ifs);
    debug_log("done.\n");

    // init linux input
    debug_log("Initializing Linux input interface for virtual keyboard...");
    vkbd_fd = init_linux_input(map);
    debug_log("done.\n");

    debug_log("Initializing Linux input interface for virtual mouse...");
    int mouse_fd = init_linux_mouse_input();
    debug_log("done.\n");

    // load keyboard touchpad
    libinput *li = nullptr;
    debug_log("Initializing Halo keyboard input interface...");

    // 1. create libinput context
    if (DEBUG) debug_log("\n    Creating udev and libinput context...\n");
    udev *udev = udev_new();
    li = libinput_udev_create_context(&interface, nullptr, udev);
    if (!li) {
        std::cerr << "Failed to create libinput context\n";
        return EXIT_FAILURE;
    }

    // 2. assign seat
    if (DEBUG) debug_log("    Assigning seat to seat0...\n");
    assert_throw(libinput_udev_assign_seat(li, "seat0") == 0);
    if (DEBUG) debug_log("    Export file descriptor...\n");
    halo_device_fd = libinput_get_fd(li);
    if (DEBUG) debug_log("    => File descriptor for device input is ", halo_device_fd, "\n");
    debug_log("done.\n");

    // start virtual keyboard handling thread
    debug_log("Initializing virtual keyboard...");
    std::thread virtual_kbd_worker(emit_key_thread);
    debug_log("done.\n");

    pollfd pfd = { halo_device_fd, POLLIN, 0 };

    debug_log("Main loop started, end handler by sending SIGINT(2) to current process (pid=", getpid(), ").\n");

    std::map < key_id_t /* key */,
        std::chrono::time_point<std::chrono::high_resolution_clock>
    > time_of_the_last_press_event;

    std::map < unsigned int /* slot */, key_id_t > slot_to_key_id_map;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    /*
     * init key press conditions
     */

    if (fn_press)
    {
        if (DEBUG) debug_log("Fn has initialization condition as pressed\n");
        fn_lock(0);
    }

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
                        debug_log("Device ", name, " (", vendor, ":", product, ") not recognized as Halo keyboard, ignored\n");
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
                            slot_to_key_id_map.emplace(slot, determined_key);
                        }
                        touchpad_mouse_handler(map, x, y, determined_key, mouse_fd, type, slot, touchpad_width, touchpad_height, next_id);
                    }
                    // key release
                    else if (type == LIBINPUT_EVENT_TOUCH_UP)
                    {
                        if (const auto key_id = slot_to_key_id_map.contains(slot) ? slot_to_key_id_map.at(slot) : -1; key_id != -1)
                        {
                            if (slot_to_key_id_map.contains(slot)) slot_to_key_id_map.erase(slot);
                            if (DEBUG) debug_log("Key ", key_id_translate(key_id), " (", key_id, ") release registered, slot=", slot, "\n");

                            std::lock_guard<std::mutex> lock(g_mutex);
                            if (const auto it = pressed_key.find(key_id);
                                it != pressed_key.end())
                            {
                                it->second.normal_press_handled = false;
                                it->second.press_down = false;
                            }
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
                                debug_log("Key ", key_id_translate(determined_key),
                                    " (", determined_key, ") press registered, slot=", slot, ", coordinate=(", x, ", ", y, ")\n");
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
                    debug_log("Key pressed but no key associated with this location in key map. "
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

    debug_log("Removing lock file...");
    if (!fs::remove(LockFilePath)) {
        debug_log("\n[WARNING] Lock file cannot be removed or doesn't exist, ignored\n");
    }
    debug_log("done.\n");
    return EXIT_SUCCESS;
}
