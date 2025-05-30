#ifndef MAP_READER_H
#define MAP_READER_H

#include <map>
#include <fstream>

struct key_location_t {
    double key_pixel_top_left_x;
    double key_pixel_top_left_y;
    double key_pixel_bottom_right_x;
    double key_pixel_bottom_right_y;
};

bool is_this_within_key_location(double x, double y, const key_location_t &);
using kbd_map = std::map < unsigned int /* key */, key_location_t >;
kbd_map read_key_map(std::ifstream &);

#endif //MAP_READER_H
