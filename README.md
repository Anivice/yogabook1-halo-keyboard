# Yogabook1 Halo Keyboard
Keyboard userspace driver for Yoga book YB1 X91F for Linux

## Introduction

Yogabook 1 keyboard is actually nothing more than a touchpad, as in it is actually a touchscreen.
By merely using the existing Linux kernel driver for touchscreens,
one can detect key presses, record their positions and determine which keys were pressed.
The exact position of each key is inside [yogabook1.map](yogabook1.map).
Fell free to edit it to match your own keyboard position.

Yogabook 1 keyboard service used unnecessarily complicated code to detect
and emit key presses as my own laptop has a terrible touchpad,
thus complicated workarounds were needed for my own use.
Still, it is not an excuse for the code quality.
Started as a service for ArchLinux with no GUI, it slowly grew into a
full-fledged keyboard driver.
Some features are removed, such as sticky keys, but their impacts
like terrible code structure persist.

Currently, the driver is only tested on ArchLinux with a generic kernel.
The driver is in an "it works" state.

## How To Use

**To obtain executable file, you can either**

### Build from source

You need to install C/C++ compilers supporting C++23,
`libinput`, `libudev`, `make` or `ninja`, and `cmake` first.
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
It has no runtime library dependencies, but you need `libinput` (from distro)
and corresponding drivers (should already be built-in in generic Linux) to listen to touchpad events.

**Then**

Copy the systemd service file to `/etc/systemd/system/halo_vkbd.service`,
executable file `halo_kbd` to `/usr/local/bin/halo_kbd`,
(optional) `ctrlword.map` to `/usr/local/etc/halo_keyboard/ctrlword.map`,
and keymap file `yogabook1.map` to `/usr/local/etc/halo_keyboard/yogabook1.map`.
Then, Start the service with `systemctl enable --now halo_vkbd.service`.

> NOTE: `ctrlword.map` allows you to use Left Contrl with Left Arrow/Right Arrow
> to skip words instead of individual characters in pure Linux console without any GUI setups.
> Useful but entirely optional.
> You can comment the `ExecStartPre=...loadkey...` line in systemd service file to remove the preloading of `ctrlword.map`.

## Currently Supported Features

Currently, the driver behaves like what it intended to do,
as in both keyboard emulation and touchpad emulation.
Tested on KDE and KDE recognizes both keyboard and touchpad
with touchpad gestures working properly.

Settings key is supported only on KDE when it's building under KDE Plasma.
AirPlane key is not supported and deemed useless anyway.

## Heads-ups

Unimplemented features are mostly related to Airplane key and Settings key.
KDE Plasma and Hyprland are the only two tested desktop environments
that seamlessly work with the driver,
and support on other desktops, particularly GNOME, is not guaranteed.

Malfunctioning on other desktops is most likely due to the presence of
both emulated keyboard and existing physical keyboard.
You need to have your desktop environment ignore the physical keyboard
to use the driver properly.
You CANNOT have libinput ignore the physical keyboard,
as the driver depends on libinput's signals to detect key presses.

## Is it suitable for daily use?

To use foul language, I started this project as a "fuck around" with ArchLinux.
I have BtrFS RAID0 setup on my Yogabook 1 to expand its storage space to 1TB.
Still, its performance is not good enough for daily use due to
the external SD card has substantial I/O bottleneck.
Nice concept laptop, though, I like this futuristic design nontheless.
