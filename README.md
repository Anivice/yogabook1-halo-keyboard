# Yogabook1 Halo Keyboard

Keyboard userspace driver for Yoga book YB1 X91F for Linux

## Introduction

Yoga book 1 keyboard is actually nothing more than a touchscreen.
Its official name is "Goodix Capacitive TouchScreen."
By merely using the existing Linux kernel driver for touchscreens,
one can detect key presses, record their positions and determine which keys were pressed.
The exact position of each key is inside [yogabook1.map](yogabook1.map).
Fell free to edit it to match your own keyboard position.

> NOTE: HOW DO I DETERMINE MY KEYBOARD POSITION?
> 
> Build the project using `CMAKE_BUILD_TYPE=Debug`,
> your keypress coordinates will be shown in debug output.
> **DO NOT** use the debug version as your normal drive as it will leak all inputs, passwords, etc.

## How To Use

**To obtain the executable file, you can either**

### Build it from source

You need to install C/C++ compilers supporting at least C++17 (GCC >= 14),
`libinput`, `libudev`, `libcap`, `make` or `ninja`, and `cmake` first.
If your compiler does not support any of the "optimization" flags,
delete them from the `CMakeLists.txt` (`set(OPTIMIZERS ...)` section):

https://github.com/Anivice/yogabook1-halo-keyboard/blob/632a8bfa927b9df2ca5501b889afc29a3c457405/CMakeLists.txt#L18-L28

They don't matter at all.

Build the driver with the following command:
```bash
    git clone https://github.com/anivice/yogabook1-halo-keyboard.git \
      && cd yogabook1-halo-keyboard && mkdir build && cd build \
      && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . --parallel $(nproc)
```

**Or**

### Download it from the release page

Release page has provided an executable file for `halo_kbd` with no support for both Airplane Key and Settings Key.
It has no runtime library dependencies (built as AppImages, using GitHub Actions), but you need `libinput` (from distro)
and corresponding drivers (generic Linux should have these already built-in) to listen to touchpad events.

**Then**

Copy the systemd service file to `/etc/systemd/system/halo_vkbd.service`,
executable file `halo_kbd` to `/usr/local/bin/halo_kbd`,
and keymap file `yogabook1.map` to `/usr/local/etc/halo_keyboard/yogabook1.map`.
Then, Start the service with `systemctl enable --now halo_vkbd.service`.

## Currently Supported Features

Currently, the driver behaves like what it intended to do,
as in both keyboard emulation and touchpad emulation.
Tested on KDE and Hyprland.

This driver assumes at most three-finger gestures.

## Heads-ups

Unimplemented features are mostly related to Airplane key and Settings key.
KDE Plasma and Hyprland are the only two tested desktop environments
that seamlessly work with the driver,
and support on other desktops, particularly GNOME, is not guaranteed.

Malfunctioning on other desktops is most likely due to the presence of
both emulated keyboard and existing touchpad.
You need to have your desktop environment ignore the touchpad
to use the driver properly.
You CANNOT have `libinput` ignore the touchpad,
as the driver depends on `libinput`'s signals to detect key presses.

**NOTE: GNOME >= 48**

GNOME >= 48 is not supported. GNOME cannot specifically ignore one input device (like any other DE) under Wayland,
and since it has already long deprecated X11, you cannot disable the touchscreen signal in Wayland
without completely nuke the device.
Disabling the input signal in `libinput` (using udev rules) will disable everyone's ability to see the device,
including this driver.
As a result, GNOME >= 48 cannot use this keyboard (under any circumstances, ever, and could & will never be fixed).
If you plan to install a modern distro, consider KDE instead.

## Is it suitable for daily use?

This keyboard driver is what you would expect from Lenovo, but without any vibration support.
It gets the job done, and should impose no substantial issues.
Not battle tested, but seems stable enough for daily use, if you ever plan to use YB1 X91F for anything, that is.

To use foul language, I started this project as a "fuck around" with ArchLinux.
I have Btrfs RAID0 setup on my Yogabook 1 to expand its storage space to 1TB.
Still, its performance is not good enough for daily use due to
the external SD card has substantial I/O bottleneck.
Nice concept laptop, though, I like this futuristic design nonetheless.
