#include "log.hpp"

std::mutex debug::log_mutex;
std::string debug::str_true = "True";
std::string debug::str_false = "False";

#if DEBUG
bool debug::do_i_show_caller_next_time = true;
#endif
