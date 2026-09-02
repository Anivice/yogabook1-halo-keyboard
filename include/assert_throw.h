#ifndef HALOKEYBOARD_ASSERT_THROW_H
#define HALOKEYBOARD_ASSERT_THROW_H

#include <stdexcept>

#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)
#define LINE_STR STRINGIZE(__LINE__)
#define assert_throw(statement) if (!(statement)) { throw std::runtime_error(__FILE__ ":" LINE_STR ":\n    " #statement ); }

#endif //HALOKEYBOARD_ASSERT_THROW_H
