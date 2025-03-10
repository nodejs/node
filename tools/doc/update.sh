#!/usr/bin/env bash

if ! [ -x $(command -v curl) ]; then
    echo no curl
    exit 1
fi

latest_commit=$(curl -s -H "Accept: application/vnd.github.VERSION.sha" "https://api.github.com/repos/nodejs/api-docs-tooling/commits/main")

rm -rf node_modules/ package-lock.json

npm install git://github.com/nodejs/api-docs-tooling#$latest_commit
