#!/bin/sh

CURL=$(command -v curl)

if ! [ -x "$CURL" ]; then
    echo curl is needed to update api doc tooling
    exit 1
fi

latest_commit=$($CURL -s -H "Accept: application/vnd.github.VERSION.sha" "https://api.github.com/repos/nodejs/api-docs-tooling/commits/main")

rm -rf node_modules/ package-lock.json

npm install "git://github.com/nodejs/api-docs-tooling#$latest_commit"
