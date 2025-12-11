'use strict';

// Tests test/wasi/wasm/clock_getres.wasm, see test/wasi/c/clock_getres.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

// This test is currently unsupported on IBM i PASE
testWasiPreview1(['clock_getres']);
