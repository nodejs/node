'use strict';
// Refs: https://github.com/nodejs/node/issues/728
const common = require('../common');
const assert = require('assert');

const EventEmitter = require('events').EventEmitter;
const e = new EventEmitter();

e.on('__proto__', common.mustCall(function(val) {
  assert.deepEqual(val, 1);
}));
e.emit('__proto__', 1);
