#!/bin/bash
if test -n "$1"; then
    user="$1"
else
    echo -n 'testling account: '
    read user
fi

curl -sSNT <(tar -cf- ../index.js run.js 2>/dev/null) \
    'http://testling.com/?main=run.js&browsers=chrome/17.0,iexplore/9.0,firefox/10.0,safari/5.1,opera/11.6' \
    -u "$user"
