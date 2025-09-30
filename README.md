

spcom - a serial console
========================

Use https://github.com/tio/tio/ instead! A lot more mature project with similar features!


BUILD
=====
On Debian/Ubuntu, install these packages:
```
apt install libserialport-dev libuv1-dev libreadline-dev
`sudo apt install build-essential`

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


