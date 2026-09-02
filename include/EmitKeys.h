#ifndef HALOKEYBOARD_EMITKEYS_H
#define HALOKEYBOARD_EMITKEYS_H

#include "map_reader.h"
#include "key_id.h"
#include "magic_enum/magic_enum.hpp"

#include <libinput.h>
#include <string_view>
#include <vector>
#include <thread>
#include <deque>
#include <mutex>
#include <condition_variable>

inline std::string_view key_id_translate(const key_id_t key) {
    return magic_enum::enum_name(key);
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
        NotificationType<long> push_notifier_; // add keys to press
        NotificationType<long> pop_notifier_;  // add keys to release
        explicit EmitKeys(int emit_fd /* keyboard fd */);
        ~EmitKeys();

        static void touchpad_mouse_handler(const kbd_map & map, double x, double y, key_id_t determined_key,
            int mouse_fd, libinput_event_type type, int slot, double touchpad_width, double touchpad_height, int & next_id);
};

#endif //HALOKEYBOARD_EMITKEYS_H
