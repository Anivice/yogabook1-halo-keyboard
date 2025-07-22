/* libmod.h
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

#ifndef LIBMOD_H
#define LIBMOD_H

#include <string>
#include <any>
#include <dlfcn.h>
#include <atomic>
#include <stdexcept>
#include <vector>

class Module
{
private:
    std::atomic < void * > handle = nullptr;
    std::mutex ModuleCallMutex;

public:
    template < typename Ret, typename... Args >
    std::any call(const std::string & function_name, Args &... args)
    {
        std::lock_guard<std::mutex> lock(ModuleCallMutex);
        void *entry = dlsym(handle, function_name.c_str());
        if (const char *error = dlerror()) {
            throw std::runtime_error(error);
        }

        using function_type = Ret(*)(Args...);
        auto func = reinterpret_cast<function_type>(entry);

        if constexpr (std::is_same_v<Ret, void>)
        {
            // If Ret is void, call the function and return nothing
            func(args...);
            return {};
        } else {
            Ret result = func(args...);
            return std::any(result);
        }
    }

    explicit Module(const std::string & module_path, std::vector<bool> & mod_map);
    ~Module() { unload(); }
    void init(std::vector<bool> & mod_map);
    void unload();
    Module & operator=(const Module &) = delete;
    [[nodiscard]] void * get_handler() const { return handle; }
};

#endif //LIBMOD_H
