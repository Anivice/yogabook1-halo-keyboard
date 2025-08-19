/* color.cpp
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

#include <string>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>
#include "color.h"

std::string get_env(const std::string & name)
{
    const char * ptr = std::getenv(name.c_str());
    if (ptr == nullptr)
    {
        return "";
    }

    return ptr;
}

std::atomic_bool color::g_no_color;

bool is_no_color()
{
    static int is_no_color_cache = -1;
    if (is_no_color_cache != -1) {
        return is_no_color_cache;
    }

    auto color_env = get_env("COLOR");
    std::ranges::transform(color_env, color_env.begin(), ::tolower);

    if (color_env == "always")
    {
        is_no_color_cache = 0;
        return false;
    }

    const bool no_color_from_env = color_env == "never" || color_env == "none" || color_env == "off"
                            || color_env == "no" || color_env == "n" || color_env == "0" || color_env == "false";
    bool is_terminal = false;
    struct stat st{};
    if (fstat(STDOUT_FILENO, &st) == -1)
    {
        is_no_color_cache = true;
    }

    if (isatty(STDOUT_FILENO))
    {
        is_terminal = true;
    }

    is_no_color_cache = no_color_from_env || !is_terminal || color::g_no_color;
    return is_no_color_cache;
}

std::string color::no_color()
{
    if (!is_no_color())
    {
        return "\033[0m";
    }

    return "";
}

std::string color::color(const int r, const int g, const int b, const int br, const int bg, const int bb)
{
    if (is_no_color())
    {
        return "";
    }

    return color(r, g, b) + bg_color(br, bg, bb);
}

std::string color::color(int r, int g, int b)
{
    if (is_no_color())
    {
        return "";
    }

    auto constrain = [](int var, const int min, const int max)->int
    {
        var = std::max(var, min);
        var = std::min(var, max);
        return var;
    };

    r = constrain(r, 0, 5);
    g = constrain(g, 0, 5);
    b = constrain(b, 0, 5);

    const int scale = 16 + 36 * r + 6 * g + b;
    return "\033[38;5;" + std::to_string(scale) + "m";
}

std::string color::bg_color(int r, int g, int b)
{
    if (is_no_color())
    {
        return "";
    }

    auto constrain = [](int var, const int min, const int max)->int
    {
        var = std::max(var, min);
        var = std::min(var, max);
        return var;
    };

    r = constrain(r, 0, 5);
    g = constrain(g, 0, 5);
    b = constrain(b, 0, 5);

    const int scale = 16 + 36 * r + 6 * g + b;
    return "\033[48;5;" + std::to_string(scale) + "m";
}
