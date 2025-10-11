'use strict';

// Tests test/wasi/wasm/sock.wasm, see test/wasi/c/sock.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['sock']);
