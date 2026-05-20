'use strict';

// Tests test/wasi/wasm/pthread.wasm, see test/wasi/c/pthread.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['--target=wasm32-wasip1-threads', 'pthread']);
