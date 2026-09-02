// This file uses unnecessarily complicated methods for key emissions because my Yogabook is too old to use "normal"
// methods, and these are workarounds to bypass certain issues (like sticky keys, etc.)

#include <algorithm>
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
#include <ranges>
#include <functional>
#include <deque>
#include <condition_variable>

#include "key_id.h"
#include "log.hpp"
#include "map_reader.h"
#include "emit_keys.h"
#include "assert_throw.h"

static std::atomic_int ctrl_c = 0;

namespace
{
    const std::unordered_map < unsigned int, const std::string > key_id_to_str_translation_table =
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

        { INVERTED_KEY_MUTE,            "Mute" },
        { INVERTED_KEY_VOLUMEDOWN,      "VolumeDown" },
        { INVERTED_KEY_VOLUMEUP,        "VolumeUp" },
        { INVERTED_KEY_AIRPLANEMODE,    "AirplaneMode" },
        { INVERTED_KEY_BRIGHTNESSDOWN,  "BrightnessDown" },
        { INVERTED_KEY_BRIGHTNESSUP,    "BrightnessUp" },
        { INVERTED_KEY_SEARCH,          "Search" },
        { INVERTED_KEY_SETTINGS,        "Settings" },
        { INVERTED_KEY_PREVIOUSSONG,    "PreviousSong" },
        { INVERTED_KEY_PLAYPAUSE,       "PlayPause" },
        { INVERTED_KEY_NEXTSONG,        "Nextsong" },
        { INVERTED_KEY_PRINT,           "Print" },

        // group 7
        { KEY_ID_MOUSELEFT, "MouseLeft" },
        { KEY_ID_MOUSERIGHT, "MouseRight" },
        { KEY_ID_TOUCHPAD, "TouchPad" },
    };

    const std::unordered_map < unsigned int, unsigned int > F1_to_F12_to_offset = {
        {KEY_ID_F1, 0},  {KEY_ID_F2, 1}, {KEY_ID_F3, 2}, {KEY_ID_F4, 3},
        {KEY_ID_F5, 4},  {KEY_ID_F6, 5}, {KEY_ID_F7, 6}, {KEY_ID_F8, 7},
        {KEY_ID_F9, 8},  {KEY_ID_F10, 9}, {KEY_ID_F11, 10}, {KEY_ID_F12, 11},
    };

    void sigint_handler(int)
    {
        constexpr char output_message[] = { 'S', 't', 'o', 'p', 'p', 'i', 'n', 'g', '.', '.', '.', '\n' };
        (void)write(1, output_message, sizeof(output_message));
        ctrl_c.store(1, std::memory_order_relaxed);
    }

    std::string_view key_id_translate(const key_id_t key)
    {
        if (const auto it = key_id_to_str_translation_table.find(key); it != key_id_to_str_translation_table.end())
            return it->second;
        return "Unknown";

    }

    template < typename Type > requires (!std::is_integral_v<Type>)
    std::vector < std::string_view > key_id_translate(const Type & keys)
    {
        std::vector < std::string_view > ret;
        ret.reserve(keys.size());
        for (const auto & key : keys) {
            static_assert(std::is_same_v < decltype(key), const key_id_t & >,
                "Decompressed element does not speaking the type `key_id_t`");
            ret.emplace_back(key_id_translate(key));
        }

        return ret;
    }

    void touchpad_mouse_handler(const kbd_map & map,
        const double x, const double y,
        const key_id_t determined_key,
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
            print("TouchPad ", key_id_translate(determined_key), " key pressed\n");
        }
        else if (determined_key == KEY_ID_TOUCHPAD || type == LIBINPUT_EVENT_TOUCH_UP)
        {
            const auto new_y = std::max(800 - static_cast<int>((x - map.at(KEY_ID_TOUCHPAD).key_pixel_top_left_x)
                / static_cast<double>(touchpad_width) * 800), 0);
            const auto new_x = std::max(static_cast<int>((y - map.at(KEY_ID_TOUCHPAD).key_pixel_top_left_y)
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
            print("TouchPad movement (", x, ", ", y, ") mapped to (", new_x, ", ", new_y, "), slot=", slot, "\n");
        }
    }

    int open_restricted(const char *path, int flags, void *user_data) {
        return open(path, flags);
    }

    void close_restricted(int fd, void *) {
        close(fd);
    }

    constexpr libinput_interface interface =
    {
        .open_restricted = open_restricted,
        .close_restricted = close_restricted,
    };

    template < typename T >
    class NotificationType {
        std::deque<T> queue_;
        std::mutex mutex_;
        std::condition_variable condition_;

    public:
        T wait()
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [&]{ return !queue_.empty(); });
            T value = std::move(queue_.front());
            queue_.pop_front();
            return value;
        }

        std::optional<T> wait_for(const uint64_t ms)
        {
            std::unique_lock lock(mutex_);
            if (!condition_.wait_for(lock, std::chrono::milliseconds(ms), [&]{ return !queue_.empty(); })) {
                return std::nullopt;
            }
            T value = std::move(queue_.front());
            queue_.pop_front();
            return value;
        }

        bool empty()
        {
            std::lock_guard lock(mutex_);
            return queue_.empty();
        }

        void push(const T value)
        {
            {
                std::lock_guard lock(mutex_);
                queue_.push_back(std::move(value));
            }

            condition_.notify_one();
        }

        void flush()
        {
            {
                std::lock_guard lock(mutex_);
                queue_.clear();
            }

            condition_.notify_one();
        }
    };

    class EmitKeys {
    private:
        std::vector<std::thread> thread_;
        const int emit_fd_;
        std::atomic_bool fn_status_{};

        std::deque<long> fn_keys_;
        std::mutex fn_keys_mutex_;

    public:
        NotificationType<long> push_notifier_;
        NotificationType<long> pop_notifier_;

        explicit EmitKeys(const int emit_fd) : emit_fd_(emit_fd)
        {
            thread_.emplace_back([this]
            {
                bool fn_lock = false;
                while (true)
                {
                    auto key = push_notifier_.wait();
                    if (key == -1) break; // -1 means QUIT, pending requests are ignored
                    if (key == KEY_ID_FN) {
                        fn_status_.store(true, std::memory_order_relaxed);
                    } else {
                        switch (key)
                        {
                            case KEY_ID_F1:
                            case KEY_ID_F2:
                            case KEY_ID_F3:
                            case KEY_ID_F4:
                            case KEY_ID_F5:
                            case KEY_ID_F6:
                            case KEY_ID_F7:
                            case KEY_ID_F8:
                            case KEY_ID_F9:
                            case KEY_ID_F10:
                            case KEY_ID_F11:
                            case KEY_ID_F12:
                                if (fn_lock || fn_status_.load(std::memory_order_relaxed))
                                {
                                    const auto new_key = F1_to_F12_list[F1_to_F12_to_offset.at(key)];
                                    print("[EmitKeys] Fn pressed, swap ", key_id_translate(static_cast<key_id_t>(key)), " to ",
                                        key_id_translate(static_cast<key_id_t>(new_key)), "\n");
                                    emit(emit_fd_, EV_KEY, new_key, 1); // press
                                    emit(emit_fd_, EV_SYN, SYN_REPORT, 0);
                                    std::lock_guard lock(fn_keys_mutex_);
                                    fn_keys_.push_back(key);
                                    key = new_key; // swap, so log can see the real key being pressed
                                }
                            break;
                            case KEY_ID_ESC:
                                if (fn_status_.load(std::memory_order_relaxed))
                                {
                                    if (fn_lock) {
                                        print("[EmitKeys] Unlocking Fn status\n");
                                        fn_lock = false;
                                    } else {
                                        print("[EmitKeys] Locking Fn status\n");
                                        fn_lock = true;
                                    }
                                }
                            break;
                            default:
                                emit(emit_fd_, EV_KEY, key, 1); // press
                                emit(emit_fd_, EV_SYN, SYN_REPORT, 0);
                            break;
                        }
                    }

                    print("[EmitKeys] Press down ", key_id_translate(static_cast<key_id_t>(key)), "\n");
                }
            });

            thread_.emplace_back([this]
            {
                while (true)
                {
                    auto key = pop_notifier_.wait();
                    if (key == -1) break; // -1 means QUIT, pending requests are ignored
                    if (key == KEY_ID_FN) {
                        fn_status_.store(false, std::memory_order_relaxed);
                    } else {
                        switch (key)
                        {
                            case KEY_ID_F1:
                            case KEY_ID_F2:
                            case KEY_ID_F3:
                            case KEY_ID_F4:
                            case KEY_ID_F5:
                            case KEY_ID_F6:
                            case KEY_ID_F7:
                            case KEY_ID_F8:
                            case KEY_ID_F9:
                            case KEY_ID_F10:
                            case KEY_ID_F11:
                            case KEY_ID_F12:
                            {
                                std::lock_guard lock(fn_keys_mutex_);
                                if (auto it = std::ranges::find(fn_keys_, key);
                                    it != fn_keys_.end())
                                {
                                    const auto new_key = F1_to_F12_list[F1_to_F12_to_offset.at(key)];
                                    print("[EmitKeys] Fn pressed, swap ", key_id_translate(static_cast<key_id_t>(key)), " to ",
                                        key_id_translate(static_cast<key_id_t>(new_key)), " for release.\n");
                                    key = new_key; // swap
                                    emit(emit_fd_, EV_KEY, key, 0); // release
                                    emit(emit_fd_, EV_SYN, SYN_REPORT, 0);
                                    fn_keys_.erase(it);
                                }
                            }
                            break;
                            default:
                                emit(emit_fd_, EV_KEY, key, 0); // release
                                emit(emit_fd_, EV_SYN, SYN_REPORT, 0);
                            break;
                        }
                    }
                    print("[EmitKeys] Release ", key_id_translate(static_cast<key_id_t>(key)), "\n");
                }
            });
        }

        ~EmitKeys()
        {
            push_notifier_.push(-1);
            pop_notifier_.push(-1);
            std::ranges::for_each(thread_, [](auto & T){ if (T.joinable()) T.join(); });
        }
    };

    class HaloKeyboard {
    private:
        kbd_map keyboard_layout_;
        int halo_device_fd_;
        int vkbd_fd_;
        int mouse_fd_;
        libinput *li_;
        udev *udev_;
        pollfd pfd_ { };

        std::thread thread_;
        std::atomic_bool running_ { true };

        std::unique_ptr<EmitKeys> emit_keys_;

        void worker()
        {
            int next_id = 0;
            const auto touchpad_width = keyboard_layout_.at(KEY_ID_TOUCHPAD).key_pixel_bottom_right_x - keyboard_layout_.at(KEY_ID_TOUCHPAD).key_pixel_top_left_x;
            const auto touchpad_height = keyboard_layout_.at(KEY_ID_TOUCHPAD).key_pixel_bottom_right_y - keyboard_layout_.at(KEY_ID_TOUCHPAD).key_pixel_top_left_y;
            std::unordered_map < unsigned int /* slot */, key_id_t > slot_to_key_id_map;

            while (running_.load(std::memory_order_relaxed))
            {
                // wait until libinput_fd is ready
                if (poll(&pfd_, 1, -1) <= 0) {
                    continue;
                }

                // tell libinput to process the pending data
                if (libinput_dispatch(li_) != 0) {
                    continue;
                }

                libinput_event *ev;
                while ((ev = libinput_get_event(li_)))
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
                            print("Device ", name, " (", vendor, ":", product, ") not recognized as Halo keyboard, ignored\n");
                            continue; // skipped the loop
                        }

                        const auto slot = static_cast<key_id_t>(libinput_event_touch_get_seat_slot(tev));
                        double x = 0.00f, y = 0.00f;
                        if (type != LIBINPUT_EVENT_TOUCH_UP) {
                            x = libinput_event_touch_get_x_transformed(tev, 1920);
                            y = libinput_event_touch_get_y_transformed(tev, 2400);
                        }

                        key_id_t determined_key;
                        // determine the key (fucking just iterate through)
                        for (const auto & [key, location] : keyboard_layout_)
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
                                    slot_to_key_id_map.emplace(slot, static_cast<key_id_t>(determined_key));
                                }
                                touchpad_mouse_handler(keyboard_layout_, x, y, determined_key, mouse_fd_, type, slot, touchpad_width, touchpad_height, next_id);
                            }
                            // key release
                            else if (type == LIBINPUT_EVENT_TOUCH_UP)
                            {
                                if (const auto key_id = slot_to_key_id_map.contains(slot) ? slot_to_key_id_map.at(slot) : -1; key_id != -1)
                                {
                                    if (slot_to_key_id_map.contains(slot)) slot_to_key_id_map.erase(slot);
                                    print("Key ", key_id_translate(static_cast<key_id_t>(key_id)), " (", key_id, ") release registered, slot=", slot, "\n");
                                    if (key_id) emit_keys_->pop_notifier_.push(key_id);
                                }
                            }
                            // key press
                            else if (type == LIBINPUT_EVENT_TOUCH_DOWN)
                            {
                                slot_to_key_id_map[slot] = determined_key;
                                print("Key ", key_id_translate(determined_key),
                                    " (", determined_key, ") press registered, slot=", slot, ", coordinate=(", x, ", ", y, ")\n");
                                if (determined_key) emit_keys_->push_notifier_.push(determined_key);
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

    public:
        explicit HaloKeyboard(const std::string & key_map)
        {
            // LOAD KEYBOARD LAYOUT
            print("Loading keymap...");
            std::ifstream ifs(key_map);
            if (!ifs.is_open()) {
                print("Unable to open file\n");
                throw std::runtime_error("Unable to open file");
            }
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
            if (!li_) {
                throw std::runtime_error("Failed to create libinput context");
            }

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

        ~HaloKeyboard()
        {
            print("Main loop stopping...\n");
            running_.store(false, std::memory_order_relaxed);
            close(halo_device_fd_);
            close(vkbd_fd_);
            close(mouse_fd_);
            if (thread_.joinable()) thread_.join();
            // delete devices
            libinput_unref(li_);
            udev_unref(udev_);
        }
    };
}

int main(int argc, char** argv)
{
    try
    {
        print<is_error>("Halo Keyboard and TouchPad userspace driver [BuildID=", BUILD_ID, ", BuildTime=", BUILD_TIME, "] version " VERSION "\n");
        if (argc != 2)
        {
            print<is_error>("Usage: ", argv[0], " <map_file>\n");
            return EXIT_FAILURE;
        }

        std::signal(SIGINT, sigint_handler);
        HaloKeyboard keyboard(argv[1]);
        while (ctrl_c.load(std::memory_order_relaxed) == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        print<is_error>(e.what(), '\n');
        return EXIT_FAILURE;
    }
}
