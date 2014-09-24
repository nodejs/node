#!/bin/bash

set -xe

rm -rf deps/npm

(cd deps && curl https://registry.npmjs.org/npm/-/npm-$1.tgz | tar xz && mv package npm)
