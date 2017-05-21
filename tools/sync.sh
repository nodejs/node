#!/usr/bin/env bash

set -e

rootdir="$(CDPATH= cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
currentbranch="$(git rev-parse --abbrev-ref HEAD)"
isrootdirty=$(git status -s)

if [[ $isrootdirty ]]
then
    git stash -u
fi

git checkout master
git fetch upstream
git rebase upstream/master

git checkout $currentbranch

if [[ $isrootdirty ]]
then
    git stash pop
fi
