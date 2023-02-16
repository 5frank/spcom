
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
    # headers needed to compile
    sudo apt-get install linux-headers-`uname -r`
    if [ ! -d "tty0tty" ]; then
        git clone "https://github.com/lcgamboa/tty0tty.git"
    fi
    cd tty0tty/module
    make
    sudo make install

}

_tty0tty_load_module()
{
    sudo depmod
    sudo modprobe tty0tty
}

_all()
{

    local HAVE_MOD="$(lsmod | grep tty0tty)"

    if [ -z "${HAVE_MOD}" ]; then
        _install_lcgamboa_tty0tty_module
        sudo depmod
        sudo modprobe tty0tty
    fi

    _show_installation_status
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
       _all
       ;;
esac
