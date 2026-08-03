'use strict';

// Tests test/wasi/wasm/getrusage.wasm, see test/wasi/c/getrusage.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['getrusage']);
