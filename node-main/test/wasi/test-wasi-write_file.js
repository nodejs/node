'use strict';

// Tests test/wasi/wasm/write_file.wasm, see test/wasi/c/write_file.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['write_file']);
