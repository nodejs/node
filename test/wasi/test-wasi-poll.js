'use strict';
require('../common');
const { testWasiPreview1 } = require('../common/wasi');

testWasiPreview1(['poll']);
