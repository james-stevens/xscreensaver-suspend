# xscreensaver-suspend

Watch `xscreensaver` and suspend the PC <N> seconds after screen power down

There's prob a better way to do this, but I couldn't find it.

This program will `watch` `xscreensaver` and issue a `systemctl suspend` command
30 mins after seeing `xscreensaver` has powered down the screens.

Any event it sees other than powering down the screens will cause it to
cancel the suspend.

Logs everything it sees & does to `syslog`

Obv, requires you have power-down enabled in "Display Power Managment" in `xscreensaver` "Advanced" settings.

## Usage
```
xscreensaver-suspend [ -t <secs-after-monitor-power-off> ] [ -s <suspend-command> ]
default: 30mins & 'systemctl suspend'
```

To use this, configure your system to automatically start it when you login,
or just start it in the background with `./xscreensaver-suspend &`.


## Build

If you have `gcc` install, it should just buuld when you run `make`.

It's very simple and only uses std *nix stuff, so shouldn't need a `configure` to compile.
