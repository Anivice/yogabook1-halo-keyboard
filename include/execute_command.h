#ifndef EXECUTE_COMMAND_H
#define EXECUTE_COMMAND_H

#include <string>
#include <vector>

struct cmd_status
{
    std::string fd_stdout; // normal output
    std::string fd_stderr; // error information
    int exit_status{}; // exit status
};

cmd_status exec_command_(const std::string &, const std::vector<std::string> &, const std::string &);

template <typename... Strings>
cmd_status exec_command(const std::string& cmd, const std::string &input, Strings&&... args)
{
    const std::vector<std::string> vec{std::forward<Strings>(args)...};
    return exec_command_(cmd, vec, input);
}

#endif //EXECUTE_COMMAND_H
