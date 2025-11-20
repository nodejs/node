'use strict';

// Tests test/wasi/wasm/getentropy.wasm, see test/wasi/c/getentropy.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['getentropy']);
