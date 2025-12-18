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

Normally, one would expect a process writing to a pipe to termiate itself when it's STDOUT is closed
by its parent process.

`xscreensaver-command --watch` does NOT do this, hence this program has to search for it in `/proc` and
send it a `SIGTERM`. This is not a big deal and debatable if its a bug, but it does add about 30% more
lines of code!

This is also becuase the `C` system call `popen()` doesn't provide anyway to get the PID of the child
process it creates. So, another solution would be to write a local version of `popen()` where you
can get the PID of the child. The amount of extra code is about the same.

If you do not workaround this issue, then the `pclose()` call will simply block forever, or until
you find & kill the child process manually.

It may be that `xscreensaver-command --watch` would detect the pipe has been closed when the next
message (state change) arrives from `xscreensaver`, but as these are quite rare and intermittant,
it just feels like its hanging forever.
