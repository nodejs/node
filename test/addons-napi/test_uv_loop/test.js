'use strict';
const common = require('../../common');
const { SetImmediate } = require(`./build/${common.buildType}/test_uv_loop`);

SetImmediate(common.mustCall());
