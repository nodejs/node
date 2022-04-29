'use strict';

require('../common');
const assert = require('assert');

const message = {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
};

assert.throws(() => Buffer.from('abc').toInt(10), message);
assert.throws(() => Buffer.from('a050').toInt(10), message);
assert.throws(() => Buffer.from('050a').toInt(10), message);
