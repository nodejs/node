#!/bin/bash
set -e
set -x
rm -rf node_modules
git checkout node_modules
node . i --ignore-scripts --no-audit
node . rebuild --ignore-scripts --no-audit
node scripts/bundle-and-gitignore-deps.js
