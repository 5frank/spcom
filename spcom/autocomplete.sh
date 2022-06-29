#!/bin/bash

# This script should go in /etc/bash_completion.d/ or where
# env $BASH_COMPLETION_COMPAT_DIR says.
# also check `complete -p` output for a list of completion scripts.
#
# Commands for degug and testing:
#   > set -x
#   > source autocomplete.sh
#

__cdb_complete_dir() {
  local cur=${COMP_WORDS[COMP_CWORD]}

  local IFS=$'\n'
  COMPREPLY=( $( compgen -o plusdirs  -f -X '!*.txt' -- $cur ) )
}

#complete -o dirnames -F _xyz xyz

_spcom_complete_opt()
{
    #echo $(spcom --autocomplete "$1" 2>/dev/null)
    COMPREPLY=( $( compgen -W "$(spcom --autocomplete $1  2>/dev/null)" -- "$cur" ) )
}

_spcom_complete_positional()
{
    case $COMP_CWORD in
        1)
            COMPREPLY=( $(compgen -W "${opts}" -- "${COMP_WORDS[COMP_CWORD]}") )
            ;;
        2)
            COMPREPLY=( $(compgen -o default -- "${COMP_WORDS[COMP_CWORD]}") )
            ;;
    esac
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
            COMPREPLY=( $( compgen -W '$(spcom --autocomplete parity  2>/dev/null)' -- "$cur" ) )
            ;;
        *)
            if [[ ${prev} != "-*" || $COMP_CWORD == 0 ]] ; then
                _spcom_complete_opt port
            elif [[ $COMP_CWORD -lt  0 ]] ; then
                _spcom_complete_opt baud
            fi
          ;;
    esac
}

complete -F _spcom_complete spcom

