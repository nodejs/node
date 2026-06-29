'use strict';

// Tests test/wasi/wasm/cant_dotdot.wasm, see test/wasi/c/cant_dotdot.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['cant_dotdot']);
