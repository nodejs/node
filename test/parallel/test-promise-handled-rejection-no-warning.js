'use strict';
const common = require('../common');

// This test verifies that DEP0018 does not occur when rejections are handled.
process.on('warning', common.mustNotCall());
process.on('unhandledRejection', common.mustCall());
Promise.reject(new Error());
