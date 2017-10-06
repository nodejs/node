'use strict';
require('../common');
const assert = require('assert');
const Signal = process.binding('signal_wrap').Signal;

// Test Signal `this` safety
// https://github.com/joyent/node/issues/6690
assert.throws(function() {
  const s = new Signal();
  const nots = { start: s.start };
  nots.start(9);
}, /^TypeError: Illegal invocation$/);
