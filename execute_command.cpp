#include "execute_command.h"
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <sys/wait.h>

/* Since pipes are unidirectional, we need three pipes:
   1. Parent writes to child's stdin
   2. Child writes to parent's stdout
   3. Child writes to parent's stderr
*/

#define NUM_PIPES           3

#define PARENT_WRITE_PIPE   0
#define PARENT_READ_PIPE    1
#define PARENT_ERR_PIPE     2

int pipes[NUM_PIPES][2];

/* Always in a pipe[], pipe[0] is for read and
   pipe[1] is for write */
#define READ_FD  0
#define WRITE_FD 1

#define PARENT_READ_FD   ( pipes[PARENT_READ_PIPE][READ_FD]   )
#define PARENT_WRITE_FD  ( pipes[PARENT_WRITE_PIPE][WRITE_FD] )
#define PARENT_ERR_FD    ( pipes[PARENT_ERR_PIPE][READ_FD]    )

#define CHILD_READ_FD    ( pipes[PARENT_WRITE_PIPE][READ_FD]  )
#define CHILD_WRITE_FD   ( pipes[PARENT_READ_PIPE][WRITE_FD]  )
#define CHILD_ERR_FD     ( pipes[PARENT_ERR_PIPE][WRITE_FD]   )

inline std::string get_errno_message(const std::string &prefix = "") {
    return prefix + std::strerror(errno);
}

cmd_status exec_command_(const std::string &cmd,
    const std::vector<std::string> &args, const std::string &input)
{
    cmd_status status = {"", "", 1}; // Default to failure

    // Initialize all required pipes
    for (auto & i : pipes)
    {
        if (pipe(i) == -1) {
            status.fd_stderr += get_errno_message("pipe() failed: ");
            status.exit_status = 1;
            return status;
        }
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        // Fork failed
        status.fd_stderr += get_errno_message("fork() failed: ");
        status.exit_status = 1;
        // Close all pipes before returning
        for (auto & pipe : pipes) {
            close(pipe[READ_FD]);
            close(pipe[WRITE_FD]);
        }
        return status;
    }

    if (pid == 0)
    {
        // Child process

        // Redirect stdin
        if (dup2(CHILD_READ_FD, STDIN_FILENO) == -1) {
            perror("dup2 stdin");
            exit(EXIT_FAILURE);
        }

        // Redirect stdout
        if (dup2(CHILD_WRITE_FD, STDOUT_FILENO) == -1) {
            perror("dup2 stdout");
            exit(EXIT_FAILURE);
        }

        // Redirect stderr
        if (dup2(CHILD_ERR_FD, STDERR_FILENO) == -1) {
            perror("dup2 stderr");
            exit(EXIT_FAILURE);
        }

        /* Close all pipe fds in the child */
        for (auto & pipe : pipes)
        {
            close(pipe[READ_FD]);
            close(pipe[WRITE_FD]);
        }

        // Build argv for execv
        std::vector<char *> argv;
        argv.push_back(const_cast<char *>(cmd.c_str()));
        for (const auto &arg : args) {
            argv.push_back(const_cast<char *>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execv(cmd.c_str(), argv.data());

        // If execv fails
        perror("execv");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent process
        // Close unused pipe ends in the parent
        close(CHILD_READ_FD);
        close(CHILD_WRITE_FD);
        close(CHILD_ERR_FD);

        // Set the write end of the stdin pipe to non-blocking to handle potential write errors
        // fcntl(PARENT_WRITE_FD, F_SETFL, O_NONBLOCK); // Optional: Depending on requirements

        // Write to child's stdin
        ssize_t total_written = 0;
        auto input_size = static_cast<ssize_t>(input.size());
        const char *input_cstr = input.c_str();
        ssize_t bytes_to_write = input_size;

        // Ensure input ends with a newline
        std::string modified_input = input;
        if (modified_input.empty() || modified_input.back() != '\n')
        {
            modified_input += "\n";
            input_cstr = modified_input.c_str();
            bytes_to_write = static_cast<ssize_t>(modified_input.size());
        }

        while (total_written < bytes_to_write)
        {
            ssize_t written = write(PARENT_WRITE_FD, input_cstr + total_written, bytes_to_write - total_written);
            if (written == -1)
            {
                if (errno == EINTR)
                    continue; // Retry on interrupt
                else {
                    status.fd_stderr += get_errno_message("write() to child stdin failed: ");
                    status.exit_status = 1;
                    return status;
                }
            }

            total_written += written;
        }

        // Optionally close the write end if no more input is sent
        if (close(PARENT_WRITE_FD) == -1)
        {
            status.fd_stderr += get_errno_message("close() PARENT_WRITE_FD failed: ");
            status.exit_status = 1;
            return status;
        }

        // Function to read all data from a file descriptor
        auto read_all = [&](int fd, std::string &output) -> bool
        {
            char buffer[4096];
            ssize_t count;
            while ((count = read(fd, buffer, sizeof(buffer))) > 0) {
                output.append(buffer, count);
            }

            if (count == -1) {
                output += get_errno_message("read() failed: ");
                return false;
            }
            return true;
        };

        // Read from child's stdout
        if (!read_all(PARENT_READ_FD, status.fd_stdout))
        {
            status.fd_stderr += get_errno_message("read_all() failed: ");
            status.exit_status = 1;
            return status;
        }

        // Read from child's stderr
        if (!read_all(PARENT_ERR_FD, status.fd_stderr))
        {
            status.fd_stderr += get_errno_message("read_all() failed: ");
            status.exit_status = 1;
            return status;
        }

        // Close the read ends
        if (close(PARENT_READ_FD) == -1)
        {
            status.fd_stderr += get_errno_message("close() PARENT_READ_FD failed: ");
            status.exit_status = 1;
            return status;
        }

        if (close(PARENT_ERR_FD) == -1)
        {
            status.fd_stderr += get_errno_message("close() PARENT_ERR_FD failed: ");
            status.exit_status = 1;
            return status;
        }

        // Wait for child process to finish
        int wstatus;
        if (waitpid(pid, &wstatus, 0) == -1)
        {
            status.fd_stderr += get_errno_message("waitpid() failed: ");
            status.exit_status = 1;
            return status;
        }
        else
        {
            if (WIFEXITED(wstatus)) {
                status.exit_status = WEXITSTATUS(wstatus);
            } else if (WIFSIGNALED(wstatus)) {
                std::ostringstream oss;
                oss << "Child terminated by signal " << WTERMSIG(wstatus) << "\n";
                status.fd_stderr += oss.str();
                status.exit_status = 1;
            } else {
                // Other cases like stopped or continued
                status.fd_stderr += "Child process ended abnormally.\n";
                status.exit_status = 1;
            }
            return status;
        }
    }
}
