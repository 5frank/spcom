#!/bin/bash

# This script should go in /etc/bash_completion.d/ or where
# env $BASH_COMPLETION_COMPAT_DIR says.
# source it to test

__cdb_complete_dir() {
  local cur=${COMP_WORDS[COMP_CWORD]}

  local IFS=$'\n'
  COMPREPLY=( $( compgen -o plusdirs  -f -X '!*.txt' -- $cur ) )
}

#complete -o dirnames -F _xyz xyz

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

    case ${COMP_WORDS[1]} in

        "-b" | "--baud" )
            COMPREPLY=( $( compgen -W '$( command spcom --autocomplete baud  2>/dev/null )' -- "$cur" ) )
            ;;

        "-p" | "--parity")
            COMPREPLY=( $( compgen -W '$( command spcom --autocomplete parity  2>/dev/null )' -- "$cur" ) )
            ;;
        *)
            if [[ ${prev} != -* ]] ; then
                COMPREPLY=( $( compgen -W '$( command spcom --autocomplete port  2>/dev/null )' -- "$cur" ) )
            else
                COMPREPLY=( $( compgen -W '$( command spcom --autocomplete baud  2>/dev/null )' -- "$cur" ) )
            fi
          ;;
    esac
}

complete -F _spcom_complete spcom

