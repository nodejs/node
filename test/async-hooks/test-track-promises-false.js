'use strict';
// Test that trackPromises: false works.
const common = require('../common');
const { createHook } = require('node:async_hooks');

createHook({
  init: common.mustNotCall(),
  trackPromises: false,
}).enable();

Promise.resolve(1729);
