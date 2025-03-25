#!/bin/sh

if ! [ -x "$NODE" ]; then
    NODE=$(command -v node)

    if ! [ -x "$NODE" ]; then
        echo 'node not found' >&2
        exit 1
    fi
fi

if ! [ -x "$NPM" ]; then
    NPM=../../deps/npm/bin/npm-cli.js

    if ! [ -x "$NPM" ]; then
        echo 'npm not found' >&2
        exit 1
    fi
fi

latest_commit=$("$NODE" ./get-latest-commit.mjs)

rm -rf node_modules/ package-lock.json

"$NODE" "$NPM" install "git://github.com/nodejs/doc-kit#$latest_commit"
