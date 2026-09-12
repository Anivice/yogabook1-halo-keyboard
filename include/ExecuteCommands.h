#ifndef HALOKEYBOARD_EXECUTECOMMANDS_H
#define HALOKEYBOARD_EXECUTECOMMANDS_H

#include <string>

struct cmd_status
{
    std::string fd_stdout; // normal output
    std::string fd_stderr; // error information
    int exit_status{}; // exit status
};

cmd_status exec_command_(const std::string &cmd, const std::vector<std::string> &args, const std::string &input);

template<typename T> concept StringLike = std::convertible_to < T, std::string >;
template<typename... Ts> concept all_string_like = (StringLike<Ts> && ...);

template < typename... Strings > requires all_string_like<Strings...>
cmd_status exec_command(const std::string &cmd, const std::string &input, Strings && ...args) {
    const std::vector < std::string > vec { std::forward<std::string>(args)... };
    return exec_command_(cmd, vec, input);
}

#endif //HALOKEYBOARD_EXECUTECOMMANDS_H
