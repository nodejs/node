/* eslint-disable node-core/required-modules */

'use strict';

const assert = require('assert');
const kLimit = Symbol('limit');
const kCallback = Symbol('callback');
const common = require('./');

class Countdown {
  constructor(limit, cb) {
    assert.strictEqual(typeof limit, 'number');
    assert.strictEqual(typeof cb, 'function');
    this[kLimit] = limit;
    this[kCallback] = common.mustCall(cb);
  }

  dec() {
    assert(this[kLimit] > 0, 'Countdown expired');
    if (--this[kLimit] === 0)
      this[kCallback]();
    return this[kLimit];
  }

  get remaining() {
    return this[kLimit];
  }
}

module.exports = Countdown;
