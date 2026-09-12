#include <unistd.h>
#include <cstring>
#include <sstream>
#include <sys/wait.h>
#include <fstream>
#include <poll.h>
#include <vector>
#include <string>
#include <cerrno>
#include "ExecuteCommands.h"

#define NUM_PIPES           3
#define PARENT_WRITE_PIPE   0
#define PARENT_READ_PIPE    1
#define PARENT_ERR_PIPE     2
#define READ_FD  0
#define WRITE_FD 1

#define PARENT_READ_FD   ( pipes[PARENT_READ_PIPE][READ_FD]   )
#define PARENT_WRITE_FD  ( pipes[PARENT_WRITE_PIPE][WRITE_FD] )
#define PARENT_ERR_FD    ( pipes[PARENT_ERR_PIPE][READ_FD]    )

#define CHILD_READ_FD    ( pipes[PARENT_WRITE_PIPE][READ_FD]  )
#define CHILD_WRITE_FD   ( pipes[PARENT_READ_PIPE][WRITE_FD]  )
#define CHILD_ERR_FD     ( pipes[PARENT_ERR_PIPE][WRITE_FD]   )

cmd_status exec_command_(
    const std::string &cmd,
    const std::vector<std::string> &args,
    const std::string &input) {
    cmd_status status{"", "", 1};   // fail by default
    int pipes[3][2];                // stdin, stdout, stderr

    // Create all three pipes
    for (auto & i : pipes) {
        if (pipe(i) == -1) {
            status.fd_stderr = std::strerror(errno);
            return status;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        status.fd_stderr = std::strerror(errno);
        for (auto &p : pipes) { close(p[0]); close(p[1]); }
        return status;
    }

    if (pid == 0) {
        // ---- Child ----
        dup2(pipes[0][0], STDIN_FILENO);
        dup2(pipes[1][1], STDOUT_FILENO);
        dup2(pipes[2][1], STDERR_FILENO);
        for (auto &p : pipes) { close(p[0]); close(p[1]); }

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(cmd.c_str()));
        for (auto &a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(cmd.c_str(), argv.data());
        perror("execv");
        _exit(127);
    }

    // ---- Parent ----
    close(pipes[0][0]);  // child stdin read end
    close(pipes[1][1]);  // child stdout write end
    close(pipes[2][1]);  // child stderr write end

    int fd_stdin  = pipes[0][1];   // write to child
    int fd_stdout = pipes[1][0];   // read from child
    int fd_stderr = pipes[2][0];   // read from child

    // Guard: always wait for child on scope exit
    auto wait_child = [&]() {
        if (pid > 0) {
            int wstatus;
            if (waitpid(pid, &wstatus, 0) == -1) {
                status.fd_stderr += "waitpid failed: ";
                status.fd_stderr += std::strerror(errno);
            } else {
                if (WIFEXITED(wstatus))
                    status.exit_status = WEXITSTATUS(wstatus);
                else if (WIFSIGNALED(wstatus))
                    status.fd_stderr += "Child killed by signal " +
                                        std::to_string(WTERMSIG(wstatus));
                else
                    status.fd_stderr += "Child ended abnormally";
            }
        }
    };

    // Ensure wait_child() runs even on early returns
    // (in production use a proper RAII wrapper)
    // Prepare input, add newline if missing
    std::string in = input;
    if (in.empty() || in.back() != '\n') in.push_back('\n');
    const char *in_ptr = in.data();
    auto remaining  = static_cast<ssize_t>(in.size());

    bool stdin_closed = false;
    bool stdout_closed = false;
    bool stderr_closed = false;

    while (!stdout_closed || !stderr_closed)
    {
        std::vector<pollfd> fds;
        if (!stdin_closed) {
            fds.push_back({fd_stdin, POLLOUT, 0});
        }
        if (!stdout_closed) {
            fds.push_back({fd_stdout, POLLIN, 0});
        }
        if (!stderr_closed) {
            fds.push_back({fd_stderr, POLLIN, 0});
        }

        if (int ret = poll(fds.data(), fds.size(), -1); ret < 0) {
            if (errno == EINTR) continue;
            status.fd_stderr = "poll failed: " + std::string(strerror(errno));
            wait_child();
            return status;
        }

        for (auto &p : fds) {
            if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                // pipe error or hangup – close and stop monitoring
                close(p.fd);
                if (p.fd == fd_stdin)  stdin_closed = true;
                if (p.fd == fd_stdout) stdout_closed = true;
                if (p.fd == fd_stderr) stderr_closed = true;
                continue;
            }

            if (p.fd == fd_stdin && (p.revents & POLLOUT))
            {
                if (const ssize_t n = write(fd_stdin, in_ptr, remaining); n > 0) {
                    in_ptr += n;
                    remaining -= n;
                    if (remaining == 0) {
                        close(fd_stdin);
                        stdin_closed = true;
                    }
                } else if (n < 0 && errno != EAGAIN && errno != EINTR) {
                    status.fd_stderr = "write to child failed: " +
                                       std::string(strerror(errno));
                    wait_child();
                    return status;
                }
            }

            if (p.fd == fd_stdout && (p.revents & POLLIN)) {
                char buf[4096];
                ssize_t n = read(fd_stdout, buf, sizeof(buf));
                if (n > 0) {
                    status.fd_stdout.append(buf, n);
                } else if (n == 0) {
                    close(fd_stdout);
                    stdout_closed = true;
                } else if (errno != EAGAIN && errno != EINTR) {
                    status.fd_stderr = "read stdout failed: " +
                                       std::string(strerror(errno));
                    wait_child();
                    return status;
                }
            }

            if (p.fd == fd_stderr && (p.revents & POLLIN)) {
                char buf[4096];
                ssize_t n = read(fd_stderr, buf, sizeof(buf));
                if (n > 0) {
                    status.fd_stderr.append(buf, n);
                } else if (n == 0) {
                    close(fd_stderr);
                    stderr_closed = true;
                } else if (errno != EAGAIN && errno != EINTR) {
                    status.fd_stderr = "read stderr failed: " +
                                       std::string(strerror(errno));
                    wait_child();
                    return status;
                }
            }
        }
    }

    wait_child();
    return status;
}
