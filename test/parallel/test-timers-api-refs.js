'use strict';
const common = require('../common');
const assert = require('assert');

// don't verify the globals for this test
common.globalCheck = false;

// try overriding global APIs to make sure
// they're not relied on by the timers
global.clearTimeout = assert.fail;

// run timeouts/intervals through the paces
const intv = setInterval(function() {}, 1);

setTimeout(function() {
  clearInterval(intv);
}, 100);

setTimeout(function() {}, 2);

