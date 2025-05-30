#ifndef EMIT_KEYS_H
#define EMIT_KEYS_H

#include <cstdint>
#include <map_reader.h>
#include <stdexcept>

void emit(int fd, uint16_t type, uint16_t code, int32_t value);
int init_linux_input(const kbd_map & key_map);
#define assert_throw(statement) if (!(statement)) { throw std::runtime_error(#statement); }

#endif //EMIT_KEYS_H
