#!/bin/bash

# This script should go in /etc/bash_completion.d/ or where
# env $BASH_COMPLETION_COMPAT_DIR says.
# also check `complete -p` output for a list of completion scripts.
#
# Commands for degug and testing:
#   > set -x
#   > source autocomplete.sh
#

#complete -o dirnames -F _xyz xyz

_spcom_complete_opt()
{
    #echo $(spcom --autocomplete "$1" 2>/dev/null)
    COMPREPLY=( $( compgen -W "$(spcom --autocomplete $1  2>/dev/null)" -- "$cur" ) )
}


_spcom_complete_positional()
{
    # this is a hack - turns out to be harder then it seems to complete
    # more then one positional argument...

    local cur prev
    cur=${COMP_WORDS[COMP_CWORD]}
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    local isdigit

    case "$cur" in
        ''|*[!0-9]*)
            isdigit=0;
        ;;
        *)
            isdigit=1;
        ;;
    esac
    # can not have negative baud. need to enter at least one digit for baud compleation
    if [[ ${prev} != "-*" && $isdigit == 1 ]] ; then
        _spcom_complete_opt baud
    else
        _spcom_complete_opt port
    fi
    return;


    if [[ $COMP_CWORD == 0 ]] ; then
                _spcom_complete_opt port
    elif [[ $isdigit ==  1 ]] ; then
                _spcom_complete_opt baud
    fi
}

_spcom_complete()
{
    local cur prev 
    cur=${COMP_WORDS[COMP_CWORD]}
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    COMPREPLY=()

    #local COMPLETES="cd add rm mv ls"
    #if [ $COMP_CWORD -eq 1 ] ; then
    #COMPREPLY=( $(compgen -W "$COMPLETES" -- $cur) )
        #return
    #fi

    case ${prev} in

        "--port")
            _spcom_complete_opt port
            ;;

        "-b" | "--baud" )
            _spcom_complete_opt baud
            ;;

        "-p" | "--parity")
            _spcom_complete_opt parity
            ;;
        *)
            _spcom_complete_positional

          ;;
    esac
}

complete -F _spcom_complete spcom

