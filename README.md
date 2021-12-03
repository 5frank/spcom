
spcom - a serial console
========================


Features:
- [ ] Command mode. Press CTRL-B (default key mapping) to enter. Example send
  raw hex and toggle DTR. This is also "scriptable" from command line options
  which allow for sending a string after serial port is opened.

- [ ] Canonical or raw mode.

- [ ] handle frequent disconnect and reconnect of serial device.
  never miss boot log message and no need to reconnect after power cycle.

- [x] sticky/async prompt. Handle async output to stdout without interrupting user input.

- [ ] Cross platform. Built upon
  [sigroks/libserialport](https://sigrok.org/wiki/Libserialport) and
  [libuv](https://libuv.org/) - both supported on GNU/Linux, MacOS, BSD,
  Android and more (currently windows need some extra workarounds).

- [ ] Sanitization of data before printed to stdout (does not mess up terminal settings).

- [ ] autocomplete on serial devices and common baudrates

- [x] list serial devices. try `spcom -lvv` for more verbose output.



BUILD
=====
On Debian/Ubuntu, install these packages:
```
apt install libserialport-dev libuv1-dev libreadline-dev
```

```
cd spcom
mkdir build
cd build
cmake ..
make
```

clean:
```
cmake [--build <build dir>] --target clean
```

with clang:
```
# clean build dir first
cmake --build build --target clean
cd build/

# should also set CMAKE_CXX_COMPILER!?
cmake -D CMAKE_C_COMPILER=/usr/bin/clang-7 ..
make
```



Alternatives
============

- [picocom](https://github.com/npat-efault/picocom) most notable mention. Currently missing support for canonical input.
- [screen](https://linux.die.net/man/1/screen) - do not handle high baudrate 
- [minicom](https://linux.die.net/man/1/minicom) - menu driven.
- [putty](https://www.putty.org/) windows and unix support.
- [gtkterm](https://github.com/Jeija/gtkterm)
- [cu](https://linux.die.net/man/1/cu)
- [conman](https://github.com/dun/conman) similar to picocom
- [necon](https://github.com/aitjcize/neocon) have "watch functionality"
- [pyserial/tools/miniterm](https://github.com/pyserial/pyserial)
- something similar might be achivable with a combination of `stty`, `socat` and `inotify`. 
- [grabserial](https://github.com/tbird20d/grabserial) python


