/* log.hpp
 *
 * Copyright 2025 Anivice Ives
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LOG_HPP
#define LOG_HPP

#include <iostream>
#include <mutex>
#include <map>
#include <unordered_map>
#include <atomic>
#include <iomanip>
#include <ranges>
#include <vector>
#include <tuple>      // for std::tuple, std::make_tuple
#include <utility>    // for std::forward
#include <any>
#include <cstring>
#include <regex>
#include <chrono>
#include <functional>
#include <cstddef>
#include <type_traits>
#include <source_location>
#include "color.h"

#define construct_simple_type_compare(type)                             \
    template <typename T>                                               \
    struct is_##type : std::false_type {};                              \
    template <>                                                         \
    struct is_##type<type> : std::true_type { };                        \
    template <typename T>                                               \
    constexpr bool is_##type##_v = is_##type<T>::value;

namespace debug {
    template <typename T, typename = void>
    struct is_container : std::false_type
    {
    };

    template <typename T>
    struct is_container<T,
        std::void_t<decltype(std::begin(std::declval<T>())),
        decltype(std::end(std::declval<T>()))>> : std::true_type
    {
    };

    template <typename T>
    constexpr bool is_container_v = is_container<T>::value;

    template <typename T>
    struct is_map : std::false_type
    {
    };

    template <typename Key, typename Value>
    struct is_map<std::map<Key, Value>> : std::true_type
    {
    };

    template <typename T>
    constexpr bool is_map_v = is_map<T>::value;

    template <typename T>
    struct is_unordered_map : std::false_type
    {
    };

    template <typename Key, typename Value, typename Hash, typename KeyEqual,
        typename Allocator>
    struct is_unordered_map<
        std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>>
        : std::true_type
    {
    };

    template <typename T>
    constexpr bool is_unordered_map_v = is_unordered_map<T>::value;

    template <typename T>
    struct is_string : std::false_type
    {
    };

    template <>
    struct is_string<std::string> : std::true_type
    {
    };

    template <>
    struct is_string<const char*> : std::true_type
    {
    };

    template <>
    struct is_string<std::string_view> : std::true_type
    {
    };

    template <typename T>
    constexpr bool is_string_v = is_string<T>::value;

    construct_simple_type_compare(bool);

    inline class move_front_t {} move_front;
    construct_simple_type_compare(move_front_t);

    inline class cursor_off_t {} cursor_off;
    construct_simple_type_compare(cursor_off_t);

    inline class cursor_on_t {} cursor_on;
    construct_simple_type_compare(cursor_on_t);

    inline class debug_log_t {} debug_log;
    construct_simple_type_compare(debug_log_t);

    inline class info_log_t {} info_log;
    construct_simple_type_compare(info_log_t);

    inline class warning_log_t {} warning_log;
    construct_simple_type_compare(warning_log_t);

    inline class error_log_t {} error_log;
    construct_simple_type_compare(error_log_t);

    template <typename T>
    struct is_pair : std::false_type
    {
    };

    template <typename Key, typename Value>
    struct is_pair<std::pair<Key, Value>> : std::true_type
    {
    };

    template <typename T>
    constexpr bool is_pair_v = is_pair<T>::value;

    extern std::mutex log_mutex;
    extern unsigned int log_level;
    extern std::atomic_uint filter_level;
    extern bool endl_found_in_last_log;

    template <typename ParamType>
    void _log(const ParamType& param);
    template <typename ParamType, typename... Args>
    void _log(const ParamType& param, const Args&... args);

    /////////////////////////////////////////////////////////////////////////////////////////////
    template <typename Container>
	requires (debug::is_container_v<Container> &&
		!debug::is_map_v<Container> &&
        !debug::is_unordered_map_v<Container>)
	void print_container(const Container& container)
    {
        constexpr uint64_t max_elements = 8;
        uint64_t num_elements = 0;
        _log("[");
        for (auto it = std::begin(container); it != std::end(container); ++it)
        {
            if (num_elements == max_elements)
            {
                _log("\n");
                _log("    ");
                num_elements = 0;
            }

            if (sizeof(*it) == 1 /* 8bit data width */)
            {
                const auto & tmp = *it;
                _log("0x", std::hex, std::setw(2), std::setfill('0'),
                    static_cast<unsigned>((*(uint8_t *)(&tmp)) & 0xFF)); // ugly workarounds
            }
            else {
                _log(*it);
            }

            if (std::next(it) != std::end(container)) {
                _log(", ");
            }

            num_elements++;
        }
        _log("]");
    }

	template <typename Map>
	requires (debug::is_map_v<Map> || debug::is_unordered_map_v<Map>)
	void print_container(const Map & map)
    {
    	_log("{");
    	for (auto it = std::begin(map); it != std::end(map); ++it)
    	{
    		_log(it->first);
    		_log(": ");
    		_log(it->second);
    		if (std::next(it) != std::end(map)) {
    			_log(", ");
    		}
    	}
    	_log("}");
    }

    template <typename ParamType> void _log(const ParamType& param)
    {
        // NOLINTBEGIN(clang-diagnostic-repeated-branch-body)
        if constexpr (debug::is_string_v<ParamType>) { // if we don't do it here, it will be assumed as a container
            std::cerr << param;
        }
        else if constexpr (debug::is_container_v<ParamType>) {
            debug::print_container(param);
        }
        else if constexpr (debug::is_bool_v<ParamType>) {
            std::cerr << (param ? "True" : "False");
        }
        else if constexpr (debug::is_pair_v<ParamType>) {
            std::cerr << "<";
            _log(param.first);
            std::cerr << ": ";
            _log(param.second);
            std::cerr << ">";
        }
        else if constexpr (debug::is_move_front_t_v<ParamType>) {
            std::cerr << "\033[F\033[K";
        }
        else if constexpr (debug::is_cursor_off_t_v<ParamType>) {
            std::cerr << "\033[?25l";
        }
        else if constexpr (debug::is_cursor_on_t_v<ParamType>) {
            std::cerr << "\033[?25h";
        }
        else if constexpr (debug::is_debug_log_t_v<ParamType>) {
            log_level = 0;
        }
        else if constexpr (debug::is_info_log_t_v<ParamType>) {
            log_level = 1;
        }
        else if constexpr (debug::is_warning_log_t_v<ParamType>) {
            log_level = 2;
        }
        else if constexpr (debug::is_error_log_t_v<ParamType>) {
            log_level = 3;
        }
        else {
            std::cerr << param;
        }
        // NOLINTEND(clang-diagnostic-repeated-branch-body)
    }

    template <typename ParamType, typename... Args>
    void _log(const ParamType& param, const Args &...args)
    {
        _log(param);
        (_log(args), ...);
    }

    template < typename StringType >
    requires (std::is_same_v<StringType, std::string> || std::is_same_v<StringType, std::string_view>)
    bool _do_i_show_caller_next_time_(const StringType & str)
    {
        return (!str.empty() && str.back() == '\n');
    }

    inline bool _do_i_show_caller_next_time_(const char * str)
    {
        return (strlen(str) > 0 && str[strlen(str) - 1] == '\n');
    }

    inline bool _do_i_show_caller_next_time_(const char c)
    {
        return (c == '\n');
    }

    template<typename T>
    struct is_char_array : std::false_type {};

    template<std::size_t N>
    struct is_char_array<char[N]> : std::true_type {};

    template<std::size_t N>
    struct is_char_array<const char[N]> : std::true_type {};

    template<std::size_t... Is, class Tuple>
    constexpr auto tuple_drop_impl(Tuple&& t, std::index_sequence<Is...>) {
        // shift indices by 1 to skip the head
        return std::forward_as_tuple(std::get<Is + 1>(std::forward<Tuple>(t))...);
    }

    template<class Tuple>
    constexpr auto tuple_drop1(Tuple&& t) {
        constexpr std::size_t N = std::tuple_size_v<std::remove_reference_t<Tuple>>;
        static_assert(N > 0, "cannot drop from empty tuple");
        return tuple_drop_impl(std::forward<Tuple>(t), std::make_index_sequence<N - 1>{});
    }

    template <typename T, typename Tag>
    struct strong_typedef
    {
        T value;
        strong_typedef() = default;
        explicit strong_typedef(T v) : value(std::move(v)) {}
        T& get() noexcept { return value; }
        [[nodiscard]] const T& get() const noexcept { return value; }
        explicit operator T&() noexcept { return value; }
        explicit operator const T&() const noexcept { return value; }
    };
    template <typename T, typename Tag> void _log(const strong_typedef<T, Tag>&) { }

    struct prefix_string_tag {};
    using prefix_string_t = strong_typedef<std::string, prefix_string_tag>;

    template <typename... Args> void log(const Args &...args)
    {
        std::lock_guard lock(log_mutex);
        static_assert(sizeof...(Args) > 0, "log(...) requires at least one argument");
        auto ref_tuple = std::forward_as_tuple(args...);
        using LastType = std::tuple_element_t<sizeof...(Args) - 1, std::tuple<Args...>>;
        using FirstType = std::tuple_element_t<0, std::tuple<Args...>>;
        const std::any last_arg = std::get<sizeof...(Args) - 1>(ref_tuple);
        std::string user_prefix_addon;

        auto call_log = []<class... Ts>(Ts&&... xs) -> decltype(auto) {
            return _log(std::forward<Ts>(xs)...);
        };

        if constexpr (std::is_same_v<FirstType, prefix_string_t>) // [PREFIX] [...]
        {
            const std::any first_arg = std::get<0>(ref_tuple);
            user_prefix_addon = std::any_cast<prefix_string_t>(first_arg).value;
        }

        if (endl_found_in_last_log)
        {
            if (!user_prefix_addon.empty()) // Addon and second elements are all present
            {
                if constexpr (sizeof...(Args) >= 2)
                {
                    using SecondType = std::tuple_element_t<1, std::tuple<Args...>>;
                    // instead of checking the first, we check the second
                    if constexpr (debug::is_debug_log_t_v<SecondType>) {
                        log_level = 0;
                    }
                    else if constexpr (debug::is_info_log_t_v<SecondType>) {
                        log_level = 1;
                    }
                    else if constexpr (debug::is_warning_log_t_v<SecondType>) {
                        log_level = 2;
                    }
                    else if constexpr (debug::is_error_log_t_v<SecondType>) {
                        log_level = 3;
                    }
                }
            }
            else // else, we check the first for the presence of log level
            {
                if constexpr (debug::is_debug_log_t_v<FirstType>) {
                    log_level = 0;
                }
                else if constexpr (debug::is_info_log_t_v<FirstType>) {
                    log_level = 1;
                }
                else if constexpr (debug::is_warning_log_t_v<FirstType>) {
                    log_level = 2;
                }
                else if constexpr (debug::is_error_log_t_v<FirstType>) {
                    log_level = 3;
                }
            }

            const auto now = std::chrono::system_clock::now();
            std::string prefix;
            switch (log_level)
            {
                case 0: prefix =  color::color(1,1,1) + "[DEBUG]"; break;
                case 1: prefix =  color::color(5,5,5) + "[INFO]"; break;
                case 2: prefix =  color::color(0,5,5) + "[WARNING]"; break;
                case 3: prefix =  color::color(5,0,0) + "[ERROR]"; break;
                default: prefix = color::color(5,5,5) + "[INFO]"; break;
            }

            // now, check if the log level is printable or masked
            if (log_level < filter_level)
            {
                return;
            }

            _log(color::color(0, 2, 2), std::format("{:%d-%m-%Y %H:%M:%S}", now), " ",
                user_prefix_addon,
                prefix, ": ", color::no_color());
        }

        if constexpr (!is_char_array<LastType>::value)
        {
            endl_found_in_last_log = _do_i_show_caller_next_time_(std::any_cast<LastType>(last_arg));
        }
        else
        {
            endl_found_in_last_log = _do_i_show_caller_next_time_(std::any_cast<const char*>(last_arg));
        }

        if (user_prefix_addon.empty())
        {
            _log(args...);
        }
        else if constexpr (sizeof...(Args) >= 2)
        {
            std::apply(call_log, tuple_drop1(ref_tuple));
        }
    }

    std::string strip_func_name(const std::string &);

}

#define print_log(...)  (void)::debug::log(debug::prefix_string_t(color::color(2,3,4) + "(" + debug::strip_func_name(std::source_location::current().function_name()) + ") "), __VA_ARGS__)
#define DEBUG_LOG       (debug::debug_log)
#define INFO_LOG        (debug::info_log)
#define WARNING_LOG     (debug::warning_log)
#define ERROR_LOG       (debug::error_log)

#endif // LOG_HPP
