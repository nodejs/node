'use strict';

// Tests test/wasi/wasm/preopen_populates.wasm, see test/wasi/c/preopen_populates.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['preopen_populates']);
