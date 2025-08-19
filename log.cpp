/* log.cpp
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

#include "log.hpp"
#include <regex>
#include <ranges>
#include <algorithm>

std::mutex debug::log_mutex;
std::atomic_uint debug::filter_level = !!!DEBUG;
unsigned int debug::log_level = 1;
bool debug::endl_found_in_last_log = true;
std::ostream * debug::output = nullptr;
std::string debug::strip_func_name(const std::string & name)
{
    const std::regex pattern(R"([\w]+ (.*)\(.*\))");
    if (std::smatch matches; std::regex_match(name, matches, pattern) && matches.size() > 1) {
        return matches[1];
    }
    return name;
}

class init_instance_t
{
public:
    init_instance_t()
    {
        if (const auto log_level_env = std::getenv("LOG_LEVEL"); log_level_env != nullptr)
        {
            try {
                debug::filter_level = std::stoi(log_level_env, nullptr, 10);
            } catch (...) {
                debug::filter_level = !!!DEBUG;
            }

            if (debug::filter_level > 3)
            {
                debug::filter_level = 3;
            }
        }

        debug::output = &std::cout;
        if (const auto log_level_env = std::getenv("LOG_OUTPUT"); log_level_env != nullptr)
        {
            std::string log_output = log_level_env;
            std::ranges::transform(log_output, log_output.begin(), ::tolower);
            if (log_output == "stderr")
            {
                debug::output = &std::cerr;
            }
            else
            {
                debug::output = &std::cout;
            }
        }
    }
} log_init_instance;
