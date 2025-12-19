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
xscreensaver-suspend [ -t <secs-after-monitor-stand-by> ] [ -l <log-level> ] [ -w <watch-command> ] [ -e ] &
    -t <secs> ... default: suspend 30mins after monitor stand-by
    -l <log-level> ... use `-l 5` to only log significant state changes, LOG_NOTICE
    -w <cmd> ... watch command is `exec xscreensaver-command --watch`, using other values is useful for debug only
    -e ... also syslog to stderr
```

To run it either configure your system to automatically start it when you login,
or just start it in the background with `./xscreensaver-suspend &`.


## Build

If you have `gcc` install, it should just build when you run `make`.

It's very simple and only uses std *nix stuff, so shouldn't need a `configure` to compile.

It will take virtually zero CPU during usage as it's spends most of it's life waiting for
messages from `xscreensaver`.

## Notes

This ended up a lot longer than orginally, even tho the original code actually worked, but 
had minor glitches.

Glitch-1: the `popen()` call in libc was giving a SEGV every so often for no apparent reason. `gdb` said it
was trying to `free()` an invalid pointer, e.g. it was already freed or just a random number.

Glitch-2: `xscreensaver-command --watch` doesn't notice the pipe it has been writing to has closed
until the next message comes in. This could take hours, so `pclose()` could block for a long time.
The way round this is to send it a `SIGTERM`, but you need to know its PID and the standard libc `popen()`
does not provide a way to get the PID of the child it creates.
