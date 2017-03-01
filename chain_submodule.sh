#! /bin/bash

# Deals with submodule stuff for libchain
# Run in the top-level directory of the app repository (i.e. app-blinker-chain/)
#
# Comamnds:
#    init - Removes the libchain submodule and adds multi-thread-chain as a
#           submodule in the same place, then prints the latest commit to
#           libchain
#  update - updates the libchain submodule to be the latest version
#
# Neil Ryan, <nryan@andrew.cmu.edu>

set -e

cmd=$1
currDir=$(pwd)

if [[ -n "$cmd" ]]; then
    if [ "$cmd" == "init" ]; then
        # Deinit the pointer to CMUAbstract/libchain
        git submodule deinit ext/libchain
        git rm ext/libchain
        # Remove the pointer from git
        rm -rf .git/modules/ext/libchain
        # Add our libchain into ext/libchain
        git submodule add https://github.com/soctar/multi-thread-chain.git ext/libchain
        # Pull the submodule
        git submodule update --init
        # Print the message for the last commit in our libchain
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
