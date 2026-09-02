#include <atomic>
#include <unistd.h>
#include <csignal>
#include "log.hpp"
#include "HaloKeyboard.h"

static std::atomic_int ctrl_c = 0;

namespace
{
    void sigint_handler(int)
    {
        constexpr char output_message[] = { 'S', 't', 'o', 'p', 'p', 'i', 'n', 'g', '.', '.', '.', '\n' };
        (void)write(1, output_message, sizeof(output_message));
        ctrl_c.store(1, std::memory_order_relaxed);
    }
}

int main(int argc, char** argv)
{
    print<is_error>("Halo Keyboard and TouchPad userspace driver [BuildID=", BUILD_ID, ", BuildTime=", BUILD_TIME, "] version " VERSION "\n");
    if (argc != 2)
    {
        print<is_error>("Usage: ", argv[0], " <map_file>\n");
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, sigint_handler);
    HaloKeyboard keyboard(argv[1]);
    while (ctrl_c.load(std::memory_order_relaxed) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return EXIT_SUCCESS;
}
