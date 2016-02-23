'use strict';
require('../common');
var assert = require('assert');
var Signal = process.binding('signal_wrap').Signal;

// Test Signal `this` safety
// https://github.com/joyent/node/issues/6690
assert.throws(function() {
  var s = new Signal();
  var nots = { start: s.start };
  nots.start(9);
}, TypeError);
