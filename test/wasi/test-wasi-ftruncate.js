'use strict';

// Tests test/wasi/wasm/ftruncate.wasm, see test/wasi/c/ftruncate.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['ftruncate']);
