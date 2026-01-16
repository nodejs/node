'use strict';
// Addons: test_uv_loop, test_uv_loop_vtable

const common = require('../../common');
const { addonPath } = require('../../common/addon-test');
const { SetImmediate } = require(addonPath);

SetImmediate(common.mustCall());
