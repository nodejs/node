'use strict';

// Tests test/wasi/wasm/gettimeofday.wasm, see test/wasi/c/gettimeofday.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['gettimeofday']);
