# Maintaining getdns

This document describes how to update `deps/getdns/`.

## 1. Obtain and extract new getdns sources

Get a new source from <https://github.com/getdnsapi/getdns>
and copy all relevant files into `deps/getdns/getdns`.

```console
% git clone https://github.com/getdnsapi/getdns
% cd getdns
% get submodule update --init --recursive
% git checkout vX.Y.Z
% cp -r ./getdns/.gitignore node/deps/getdns/getdns
% cp -r ./getdns/LICENSE node/deps/getdns/getdns
% cp -r ./getdns/cmake node/deps/getdns/getdns
% cp -r ./getdns/CMakeLists.txt node/deps/getdns/getdns
% cp -r ./getdns/getdns.pc.in node/deps/getdns/getdns
% cp -r ./getdns/getdns_ext_event.pc.in node/deps/getdns/getdns
% cp -r ./getdns/src node/deps/getdns/getdns
% rm -rf node/deps/getdns/getdns/src/test
% rm -rf node/deps/getdns/getdns/src/yxml/test
```

## 2. Update `deps/getdns/configure_file.py`

Read `deps/getdns/getdns/CMakeLists.txt` and ensure that the definitions from
it are accurately reflected in the constants list in
`deps/getdns/configure_file.py`.

## 3. Build Node.js

Build Node.js normally. If getdns has added or removed files,
`deps/getdns/getdns.gyp` may need to be updated.

## 4. Test Node.js

Run the entire test suite, including the `internet` category.

## 5. Stage the changes and commit

Ensure that things look "correct". If, for example, a bunch of test files
have been added, delete them and update the above steps.

## 6. Open a PR
