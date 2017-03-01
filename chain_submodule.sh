#! /bin/bash

# Deals with submodule stuff for libchain
# Comamnds:
#    init - Removes the libchain submodule and adds multi-thread-chain as a
#           submodule in the same place, then prints the latest commit to
#           libchain
#  update - updates the libchain submodule to be the latest version

set -e

cmd=$1
currDir=$(pwd)

if [[ -n "$cmd" ]]; then
    if [ "$cmd" == "init" ]; then
        git submodule deinit ext/libchain
        git rm ext/libchain
        #git submodule init
        #git submodule update
        git submodule add https://github.com/soctar/multi-thread-chain.git ext/libchain
        git submodule update --init
        cd ext/libchain/
        printf "$(git log --oneline -n 1)\n"
        cd $currDir
    elif [ "$cmd" == "update" ]; then
        cd ext/libchain/
        git pull origin master
        printf "$(git log --oneline -n 1)\n"
        cd $currDir
    else
        printf "Usage:\n\
        $0 init - Removes old chain, updates to point to multi-thread-chain\n\
        $0 update - Updates multi-threa-chain to point to master\n"
    fi
else
    printf "Usage:\n\
    $0 init - Removes old chain, updates to point to multi-thread-chain\n\
    $0 update - Updates multi-threa-chain to point to master\n"
fi
