#include "map_reader.h"
#include <stdexcept>
#include <sstream>
#include "log.hpp"

bool is_this_within_key_location(const double x, const double y, const key_location_t &key)
{
    return      (x >= key.key_pixel_top_left_x)
            &&  (x <= key.key_pixel_bottom_right_x)
            &&  (y >= key.key_pixel_top_left_y)
            &&  (y <= key.key_pixel_bottom_right_y);
}

void replace_all(
    std::string & original,
    const std::string & target,
    const std::string & replacement)
{
    if (target.empty()) return; // Avoid infinite loop if target is empty

    if (target == " " && replacement.empty()) {
        std::erase_if(original, [](const char c) { return c == ' '; });
    }

    size_t pos = 0;
    while ((pos = original.find(target, pos)) != std::string::npos) {
        original.replace(pos, target.length(), replacement);
        pos += replacement.length(); // Move past the replacement to avoid infinite loop
    }
}

kbd_map read_key_map(std::ifstream & file)
{
    std::string line;
    kbd_map result;
    while (getline(file, line))
    {
        std::string pair = line;
        replace_all(pair, " ", "");
        if (!pair.empty() && line.size() > 1 && line[0] != '#')
        {
            std::stringstream ss(line);
            unsigned int key{};
            key_location_t location {};
            ss >> key;
            ss >> location.key_pixel_top_left_x;
            ss >> location.key_pixel_top_left_y;
            ss >> location.key_pixel_bottom_right_x;
            ss >> location.key_pixel_bottom_right_y;
            if (!key
                || (location.key_pixel_top_left_x == 0)
                || (location.key_pixel_top_left_y == 0)
                || (location.key_pixel_bottom_right_x == 0)
                || (location.key_pixel_bottom_right_y == 0))
            {
                throw std::invalid_argument("Invalid keyboard map!");
            }

            result.emplace(key, location);
        }
    }

    return result;
}
