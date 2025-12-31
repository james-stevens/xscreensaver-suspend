# xscreensaver-suspend

This program will watch `xscreensaver` and suspend the PC 30mins after the screen is put into stand-by.

There's prob a better way to do this, but I couldn't find it. It's a shame `xscreensaver`
doesn't have this built-in.

It does this by running `xscreensaver-command --watch` and issuing a `systemctl suspend` command
30 mins after seeing `xscreensaver` has put the screen into stand-by.

Any event it sees other than putting the screen into stand-by will cause it to
cancel its suspend timer.

Logs everything it sees & does to `syslog`

Uses `systemctl is-system-running` to detect when it has come back from a suspend event.
Technically, the `systemctl suspend` actually blocks until the system wakes from the suspend, 
but using `systemctl is-system-running` means it can check everything is back & working.

Obv, requires you have stand-by enabled in "Display Power Managment" in `xscreensaver` "Advanced" settings.

Its external shell calls and the trigger event it looks for can be specified on the command line, 
so this can be used for various other purposes, e.g.

		./xscreensaver-suspend -t 5 -s "/usr/bin/make_coffee" -r "BLANK "

Makes a cup of coffee 5 seconds after the screen goes blank, as you clearly need waking up.

You should be able to run as many occurances of this as you like. Each will run its own watch on `xscreensaver`.

Triggering on the event `UNBLANK ` can be used to take actions when the screen wakes from blank / screen-saver.

For other events, check your `syslog`. I've seen `BLANK <date-time>`, `UNBLANK <date-time>` and `RUN <new-id> <old-id>`, where `RUN <num> ` is
running your screen-saver and `RUN 0 ` is putting the screen into stand-by. So `RUN 0 214` was running screen-saver `214` and is now
transitioning to stand-by.

If the PC is suspended by some other process, it tries to detect this and restart its watcher process
as the watcher can lose its connection to `xscreensaver` after a suspend. It will always
terminate its watcher before it will initiate a suspend itself.

## Usage
```
xscreensaver-suspend &
    -t <secs> ... default: suspend 30mins after monitor stand-by
    -l <log-level> ... use `-l 5` to only log significant state changes, LOG_NOTICE & above
    -w <cmd> ... watch command is `exec xscreensaver-command --watch`
    -c <cmd> ... check command is `systemctl is-system-running | logger -t xscreensaver-suspend -i`
    -s <cmd> ... suspend command is `exec systemctl suspend`
    -r <event> ... trigger event, default is `RUN 0 `
    -e ... also syslog to stderr
```

All commands will be run in a shell `/bin/sh`, so any standard shell syntax should work.

To run it either configure your system to automatically start it when you login,
or just start it in the background with `./xscreensaver-suspend &`.

You can use the script `./debug.sh` as the watch command if you want to just play about.


## Build

If you have `gcc` install, it should just build when you run `make`.

It's very simple and only uses std *nix stuff, so shouldn't need a `configure` to compile.

It will take virtually zero CPU during usage as it spends most of its life waiting for
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
