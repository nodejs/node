'use strict';

// Tests test/wasi/wasm/main_args.wasm, see test/wasi/c/main_args.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['main_args']);
