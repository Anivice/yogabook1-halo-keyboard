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
#if DEBUG
#   include <vector>
#   include <tuple>      // for std::tuple, std::make_tuple
#   include <utility>    // for std::forward
#   include <any>
#   include <cstring>
#   include <regex>
#endif

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
    struct is_string<std::basic_string<char>> : std::true_type
    {
    };

    template <>
    struct is_string<const char*> : std::true_type
    {
    };

    template <typename T>
    constexpr bool is_string_v = is_string<T>::value;

    construct_simple_type_compare(bool);

    inline class lower_case_bool_t {} lower_case_bool;
    construct_simple_type_compare(lower_case_bool_t);

    inline class upper_case_bool_t {} upper_case_bool;
    construct_simple_type_compare(upper_case_bool_t);

    inline class clear_line_t {} clear_line;
    construct_simple_type_compare(clear_line_t);

    inline class cursor_off_t {} cursor_off;
    construct_simple_type_compare(cursor_off_t);

    inline class cursor_on_t {} cursor_on;
    construct_simple_type_compare(cursor_on_t);

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
    template <typename ParamType>
    void _log(const ParamType& param);
    template <typename ParamType, typename... Args>
    void _log(const ParamType& param, const Args&... args);

    /////////////////////////////////////////////////////////////////////////////////////////////
    extern std::string str_true;
    extern std::string str_false;
    template <typename... Args> void log(const Args&... args);

    template <typename Container>
	requires (debug::is_container_v<Container> &&
		!debug::is_map_v<Container> &&
        !debug::is_unordered_map_v<Container>)
	void print_container(const Container& container)
    {
        constexpr uint64_t max_elements = 8;
        uint64_t num_elements = 0;
        std::cerr << "[";
        for (auto it = std::begin(container); it != std::end(container); ++it)
        {
            if (num_elements == max_elements)
            {
                std::cerr << "\n";
                _log("    ");
                num_elements = 0;
            }

            if (sizeof(*it) == 1 /* 8bit data width */)
            {
                const auto & tmp = *it;
                std::cerr << "0x" << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<unsigned>((*(uint8_t *)(&tmp)) & 0xFF); // ugly workarounds
            }
            else {
                _log(*it);
            }

            if (std::next(it) != std::end(container)) {
                std::cerr << ", ";
            }

            num_elements++;
        }
        std::cerr << "]";
    }

	template <typename Map>
	requires (debug::is_map_v<Map> || debug::is_unordered_map_v<Map>)
	void print_container(const Map & map)
    {
    	std::cerr << "{";
    	for (auto it = std::begin(map); it != std::end(map); ++it)
    	{
    		_log(it->first);
    		std::cerr << ": ";
    		_log(it->second);
    		if (std::next(it) != std::end(map)) {
    			std::cerr << ", ";
    		}
    	}
    	std::cerr << "}";
    }

    template <typename ParamType> void _log(const ParamType& param)
    {
        // NOLINTBEGIN(clang-diagnostic-repeated-branch-body)
        if constexpr (debug::is_string_v<ParamType>) {
            std::cerr << param;
        }
        else if constexpr (debug::is_container_v<ParamType>) {
            debug::print_container(param);
        }
        else if constexpr (debug::is_bool_v<ParamType>) {
            std::cerr << (param ? str_true : str_false);
        }
        else if constexpr (debug::is_lower_case_bool_t_v<ParamType>) {
            str_true = "true";
            str_false = "false";
        }
        else if constexpr (debug::is_upper_case_bool_t_v<ParamType>) {
            str_true = "True";
            str_false = "False";
        }
        else if constexpr (debug::is_pair_v<ParamType>) {
            std::cerr << "<";
            _log(param.first);
            std::cerr << ": ";
            _log(param.second);
            std::cerr << ">";
        }
        else if constexpr (debug::is_clear_line_t_v<ParamType>) {
            std::cerr << "\033[F\033[K";
        }
        else if constexpr (debug::is_cursor_off_t_v<ParamType>) {
            std::cerr << "\033[?25l";
        }
        else if constexpr (debug::is_cursor_on_t_v<ParamType>) {
            std::cerr << "\033[?25h";
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

#if DEBUG
    extern bool do_i_show_caller_next_time;

    template <typename... Args> void log(const char * caller, const Args &...args)
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        static_assert(sizeof...(Args) > 0, "log(...) requires at least one argument");
        auto ref_tuple = std::forward_as_tuple(args...);
        using LastType = std::tuple_element_t<sizeof...(Args) - 1, std::tuple<Args...>>;
        const std::any last_arg = std::get<sizeof...(Args) - 1>(ref_tuple);

        if (do_i_show_caller_next_time) {
            _log("[", caller, "] ");
        }

        // FIXME: this is an extremely ugly workaround. DON'T DO THIS
        // only show called if last element is some form of string type and has '\n'
        if (std::regex_match(typeid(LastType).name(), std::regex(R"(.*basic_string.*)")))
        {
            if (const auto str = std::any_cast<const std::string &>(last_arg); !str.empty()) {
                do_i_show_caller_next_time = str[str.size() - 1] == '\n';
            }
        }
        else if (std::regex_match(typeid(LastType).name(), std::regex(R"(A[\d]+\_c)")))
        {
            if (const auto str = std::any_cast<const char*>(last_arg); std::strlen(str) > 0) {
                do_i_show_caller_next_time = str[std::strlen(str) - 1] == '\n';
            }
        }
        else if (std::is_same_v<LastType, char>
            || std::is_same_v<LastType, const char>
            || std::is_same_v<LastType, const char&>)
        {
            const auto last_char = std::any_cast<char>(last_arg);
            do_i_show_caller_next_time = last_char == '\n';
        } else {
            do_i_show_caller_next_time = false;
        }
#else
    template <typename... Args> void log(const Args &...args)
    {
#endif
        _log(args...);
    }
}

#if DEBUG
#   include <source_location>
    inline std::string strip_name(const std::string & name)
    {
        const std::regex pattern(R"([\w]+ (.*)\(.*\))");
        if (std::smatch matches; std::regex_match(name, matches, pattern) && matches.size() > 1) {
            return matches[1];
        }

        return name;
    }
#   define debug_log(...) ::debug::log(strip_name(std::source_location::current().function_name()).c_str(), __VA_ARGS__)
#else
#   define debug_log(...) ::debug::log(__VA_ARGS__)
#endif

#endif // LOG_HPP
