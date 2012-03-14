#!/usr/bin/env sh

cp -fpv ~/node/common.gypi legacy/
cp -fpv ~/node/tools/addon.gypi legacy/tools/
cp -fpv ~/node/tools/gyp_addon legacy/tools/
cp -rfpv ~/node/tools/gyp legacy/tools/

# we don't care about gyp's test suite and it's rather big...
rm -rf legacy/tools/gyp/test
