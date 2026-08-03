'use strict';

// Tests test/wasi/wasm/fd_prestat_get_refresh.wasm, see test/wasi/c/fd_prestat_get_refresh.c
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['fd_prestat_get_refresh']);
