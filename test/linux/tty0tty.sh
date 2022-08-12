
# script based on project README.md

# needed for building module

_show_installation_status()
{
    lsmod | grep tty0tty
    ls /etc/udev/rules.d/*tty0tty*
    ls /dev/tnt*
}

_install_lcgamboa_tty0tty_module()
{
    sudo apt-get install linux-headers-`uname -r`
    git clone https://github.com/lcgamboa/tty0tty.git
    cd tty0tty/module
    make
    sudo make install

}

_tty0tty_load_module()
{
    sudo depmod
    sudo modprobe tty0tty
}

case "$1" in
   "install")
       _install_lcgamboa_tty0tty_module
       ;;
   "status")
       _show_installation_status
       ;;
   "load")
       _tty0tty_load_module
       ;;
   *)
       echo "args neeeded"
       ;;
esac
