#!/bin/bash
echo -n 'testling user: '
read user

stty -echo
echo -n 'password: '
read pass
stty echo

function tick () {
    cat <(echo 'process={};') ../wrappers/process.js tick.js \
        | curl -sSNT- -u "$user:$pass" testling.com
}

function util () {
    tar -cf- util.js ../builtins/util.js |
        curl -sSNT- -u "$user:$pass" \
        'http://testling.com/?main=util.js&noinstrument=builtins/util.js'
}

if test -z "$1"; then
    tick
    util
elif test "$1" == 'tick'; then
    tick
elif test "$1" == 'util'; then
    util
fi
