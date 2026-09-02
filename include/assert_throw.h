#ifndef HALOKEYBOARD_ASSERT_THROW_H
#define HALOKEYBOARD_ASSERT_THROW_H

#include <cstdlib>
#include <cstring>
#include "log.hpp"

#define assert_throw(statement)                                                                                             \
if (!(statement))                                                                                                           \
{                                                                                                                           \
    print<is_error>("Expectation " #statement " disappointed, location " __FILE__ ":", __LINE__, " errno=", strerror(errno), "\n");   \
    abort();                                                                                                                \
}

#endif //HALOKEYBOARD_ASSERT_THROW_H
