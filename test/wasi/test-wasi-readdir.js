'use strict';

// Tests test/wasi/wasm/readdir.wasm, see test/wasi/c/readdir.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['readdir']);
