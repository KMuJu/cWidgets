# cWidgets

A redo of my hyprland rice, but in c instead of astal with ts.
Uses a lot of signals rather than polling.
Only polls for the time, everything else is listening to signals or dbus.

Inspired by [jack000987/gtkbar](https://github.com/jack000987/gtkbar) and [Aylur/astal](https://github.com/Aylur/astal).

## Requirements

- gtk4 (Widgets)
- gtk4-layer-shell (Layers in hyprland)
- libnm (Networkmanager) (Network information)
- Wireplumber (Audio)
- libcjson (Hyprland socket)
- sass (Better css)

## How to run

```sh
cd build
# Compile the sass files
sass ../scss/style.scss style.css
# Compile the c code
meson compile
# Run the widgets
./cWidgets
```

```sh
# or simply
sass ../scss/style.scss style.css && meson compile && ./cWidgets
```

## Useful links

- [Bluez Adapter](https://git.kernel.org/pub/scm/bluetooth/bluez.git/tree/doc/org.bluez.Adapter.rst)
- [Bluez Device](https://git.kernel.org/pub/scm/bluetooth/bluez.git/tree/doc/org.bluez.Device.rst)
  (To know the properties of the dbus interface)
- [Libnm docs](https://networkmanager.dev/docs/libnm/latest/)
  (To know the signals different libnm objects emit, and function on them)
- [Libnm examples](https://gitlab.freedesktop.org/NetworkManager/NetworkManager/-/blob/main/examples/C/glib)
  (Examples for how to use libnm)
- [Gtk docs](https://docs.gtk.org/)
  (Has docs for every part of gtk system)
- [Wireplumber c api](https://pipewire.pages.freedesktop.org/wireplumber/library/c_api.html)
  (Docs for using wireplumber to get audio information)
- [Wpctl source code](https://github.com/PipeWire/wireplumber/blob/master/src/tools/wpctl.c)
  (Useful to see how you can use wireplumber)
- [Default nodes api](https://github.com/PipeWire/wireplumber/blob/master/modules/module-default-nodes-api.c)
  (The only place you can find the signals for the default nodes api module. Mixer api is in the same folder)

## Bugs

### Sometimes does not update bt icon in bar when disconnecting

Might only be when pc is asleep.
The dbus object manager might not have called the object removed signal,
or it might have called it for something that is not an interface.

### Cannot connect to hyprland socket after logging out (`hyprctl dispatch exit`)

It happens because the tmux session retains the old hyprland instance environment variable after loging out.

Solution:
- Restart tmux

## Performance

### Debug mode

Haven't tested it when the buildtype is optimized

Idle is when the widgets are not focused or changing much.

Active is when opening a widget and doing stuff in the window.
Such as changing the internet.

***Cpu usage:***

Idle: 0%

Active: <15%

***Memory usage:***

Idle: 2-3%
Active: 2-3%
