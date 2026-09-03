#include "EmitKeys.h"
#include "key_id.h"
#include "log.hpp"
#include "magic_enum/magic_enum.hpp"
#include "assert_throw.h"
#include "map_reader.h"

#include <unistd.h>
#include <linux/uinput.h>
#include <ranges>
#include <algorithm>

namespace
{
    void emit(const int fd, const uint16_t type, const uint16_t code, const int32_t value)
    {
        input_event ev{};
        ev.type = type;
        ev.code = code;
        ev.value = value;
        gettimeofday(&ev.time, nullptr);
        assert_throw(write(fd, &ev, sizeof(ev)) == sizeof(ev));
    }

    unsigned int F1_to_F12_to_offset(const unsigned int key)
    {
        switch (key) {
            case KEY_ID_F1: return 0;
            case KEY_ID_F2: return 1;
            case KEY_ID_F3: return 2;
            case KEY_ID_F4: return 3;
            case KEY_ID_F5: return 4;
            case KEY_ID_F6: return 5;
            case KEY_ID_F7: return 6;
            case KEY_ID_F8: return 7;
            case KEY_ID_F9: return 8;
            case KEY_ID_F10: return 9;
            case KEY_ID_F11: return 10;
            case KEY_ID_F12: return 11;
            default: assert_throw(false);
        }
    }
}

void EmitKeys::touchpad_mouse_handler(const kbd_map & map,
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

EmitKeys::EmitKeys(const int emit_fd): emit_fd_(emit_fd) {
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
                            const auto new_key = F1_to_F12_list[F1_to_F12_to_offset(key)];
                            print("[EmitKeys] Fn pressed, swap ", key_id_translate(static_cast<key_id_t>(key)), " to ",
                                  key_id_translate(static_cast<key_id_t>(new_key)), "\n");
                            emit(emit_fd_, EV_KEY, new_key, 1); // press
                            emit(emit_fd_, EV_SYN, SYN_REPORT, 0);
                            std::lock_guard lock(fn_keys_mutex_);
                            fn_keys_.push_back(key);
                            key = new_key; // swap, so log can see the real key being pressed
                        }
                        else
                        {
                            emit(emit_fd_, EV_KEY, key, 1); // press
                            emit(emit_fd_, EV_SYN, SYN_REPORT, 0);
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
                            const auto new_key = F1_to_F12_list[F1_to_F12_to_offset(key)];
                            print("[EmitKeys] Fn pressed, swap ", key_id_translate(static_cast<key_id_t>(key)), " to ",
                                  key_id_translate(static_cast<key_id_t>(new_key)), " for release.\n");
                            key = new_key; // swap
                            emit(emit_fd_, EV_KEY, key, 0); // release
                            emit(emit_fd_, EV_SYN, SYN_REPORT, 0);
                            fn_keys_.erase(it);
                        }
                        else
                        {
                            emit(emit_fd_, EV_KEY, key, 0); // release
                            emit(emit_fd_, EV_SYN, SYN_REPORT, 0);
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

EmitKeys::~EmitKeys()
{
    push_notifier_.push(-1);
    pop_notifier_.push(-1);
    std::ranges::for_each(thread_, [](auto & T){ if (T.joinable()) T.join(); });
}
