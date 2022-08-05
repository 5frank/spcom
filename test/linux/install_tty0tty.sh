
# script based on project README.md

# needed for building module

_show_installation_status()
{
    lsmod | grep tty0tty
    ls /etc/udev/rules.d/*tty0tty*
}

_install_lcgamboa_tty0tty_module()
{
    sudo apt-get install linux-headers-`uname -r`
    git clone https://github.com/lcgamboa/tty0tty.git
    cd tty0tty/module
    make
    sudo make install

}
_install_lcgamboa_tty0tty_module
