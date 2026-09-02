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
#include <sstream>
#include <vector>

class is_error {};
class is_normal {};

template < typename T1, typename T2 > void sprint_1(const std::pair<T1, T2>& val, std::ostream & oss);

template < typename  T >
void sprint_1(const T& val, std::ostream & oss) {
    oss << val;
}

template < typename T >
void sprint_1(const std::vector<T>& container, std::ostream & oss)
{
    sprint_1("{", oss);
    for (auto& elem : container) {
        sprint_1(elem, oss);
        sprint_1(", ", oss);
    }
    sprint_1("}", oss);
}

template < typename T1, typename T2 >
void sprint_1(const std::pair<T1, T2>& val, std::ostream & oss)
{
    sprint_1("[", oss);
    sprint_1(val.first, oss);
    sprint_1(", ", oss);
    sprint_1(val.second, oss);
    sprint_1("]", oss);
}

template < typename... Args >
std::string sprint(const Args &...args)
{
    std::ostringstream oss;
    (sprint_1(args, oss), ...);
    return oss.str();
}

template < typename T >
void print_1(const T & val, std::ostream & oss) {
    sprint_1(val, oss);
}

template < typename MsgType = is_normal, typename... Args >
requires (std::is_same_v<MsgType, is_normal> || std::is_same_v<MsgType, is_error>)
void print(const Args &...args)
{
    if constexpr (std::is_same_v<MsgType, is_error>) {
        (print_1(args, std::cerr), ...);
    } else if constexpr (DEBUG) {
        (print_1(args, std::cout), ...);
    }
}

#endif // LOG_HPP
