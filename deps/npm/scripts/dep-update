#!/bin/bash
node . install --save $1@$2 &&\
node scripts/gen-dev-ignores.js &&\
git add node_modules package.json package-lock.json &&\
git commit -m"$1@$2" &&\
node . repo $1 &&\
git commit --amend
