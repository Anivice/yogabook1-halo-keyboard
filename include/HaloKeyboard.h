#ifndef HALOKEYBOARD_HALOKEYBOARD_H
#define HALOKEYBOARD_HALOKEYBOARD_H

#include <thread>
#include <libinput.h>
#include <libudev.h>
#include <poll.h>
#include "EmitKeys.h"
#include "map_reader.h"

/// Halo keyboard main entity
/// This entity will create a virtual keyboard device, and create a thread to maintain that device
/// thread will be terminated when the entity is destroyed
class HaloKeyboard
{
    kbd_map keyboard_layout_;
    int halo_device_fd_;
    int vkbd_fd_;
    int mouse_fd_;
    libinput *li_;
    udev *udev_;
    pollfd pfd_ { };
    std::thread thread_; // worker thread
    std::atomic_bool running_ { true }; // running flag
    std::unique_ptr<EmitKeys> emit_keys_; // key code press/release handlers

    /// main worker, registering key presses and releases
    void worker();
public:

    /// Halo keyboard main entity
    /// @param key_map path to keyboard geometry map
    explicit HaloKeyboard(const std::string & key_map);
    ~HaloKeyboard();
};

#endif //HALOKEYBOARD_HALOKEYBOARD_H