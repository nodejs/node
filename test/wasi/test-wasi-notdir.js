'use strict';

// Tests test/wasi/wasm/notdir.wasm, see test/wasi/c/notdir.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['notdir']);
