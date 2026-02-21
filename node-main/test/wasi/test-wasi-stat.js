'use strict';

// Tests test/wasi/wasm/stat.wasm, see test/wasi/c/stat.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['stat']);
