// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { Signal } = internalBinding('signal_wrap');

// Test Signal `this` safety
// https://github.com/joyent/node/issues/6690
assert.throws(function() {
  const s = new Signal();
  const nots = { start: s.start };
  nots.start(9);
}, /^TypeError: Illegal invocation$/);
