/* libmod.cpp
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

#include "libmod.h"

Module::Module(const std::string & module_path, std::vector<bool> & mod_map)
{
    handle = dlopen(module_path.c_str(), RTLD_LAZY);
    if (!handle) {
        throw std::runtime_error(dlerror());
    }

    try {
        init(mod_map);
    } catch (const std::exception &) {
        dlclose(handle);
        throw;
    }
}

void Module::unload()
{
    if (handle)
    {
        call<void>("module_exit");
        dlclose(handle);
        handle = nullptr;
    }
}

extern "C" void normal_key_emit(unsigned int);

void Module::init(std::vector<bool> & mod_map)
{
    int register_map[13] = { };
    mod_map.resize(std::size(register_map), false);
    if (std::any_cast<int>(call<int>("module_init", normal_key_emit, register_map)) != 0)
    {
        throw std::runtime_error("Module initialization failed");
    }

    for (uint64_t i = 0; i < std::size(register_map); i++)
    {
        if (register_map[i] != 0)
        {
            mod_map[i] = true;
        }
    }
}
