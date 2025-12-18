# xscreensaver-suspend

Watch `xscreensaver` and suspend the PC 30mins after the screen is put into stand-by.

There's prob a better way to do this, but I couldn't find it. It's a shame `xscreensaver`
doesn't have this built-in.

This program will run `xscreensaver-command --watch` and issue a `systemctl suspend` command
30 mins after seeing `xscreensaver` has put the screen into stand-by.

Any event it sees other than putting the screen into stand-by will cause it to
cancel the suspend.

Logs everything it sees & does to `syslog`

Uses `systemctl is-system-running` to detect when it has come back from a suspend event.
Technically, the `systemctl suspend` actually blocks until the system wakes from the suspend, 
but using `systemctl is-system-running` means it can check everything is back & working.

Obv, requires you have stand-by enabled in "Display Power Managment" in `xscreensaver` "Advanced" settings.

## Usage
```
xscreensaver-suspend [ -t <secs-after-monitor-stand-by> ]
default: 30mins
```

To run it either configure your system to automatically start it when you login,
or just start it in the background with `./xscreensaver-suspend &`.


## Build

If you have `gcc` install, it should just build when you run `make`.

It's very simple and only uses std *nix stuff, so shouldn't need a `configure` to compile.

It will take virtually zero CPU during usage as it's spends most of it's life waiting for
messages from `xscreensaver`.

## Note on `xscreensaver-command --watch`

Normally, one would expect a process in a pipe to termiate itself when it's STDIN is closed.

`xscreensaver-command --watch` does NOT do this, hence this program has to look for it in `/proc` and
send it a `SIGTERM`. This is not a big deal and debatable if its a bug, but it does mean about double
the lines of code!
