'use strict';
// Refs: https://github.com/nodejs/node/issues/728
const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');
const ee = new EventEmitter();

ee.on('__proto__', common.mustCall((data) => {
  assert.strictEqual(data, 42);
}));

ee.emit('__proto__', 42);
