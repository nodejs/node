'use strict';
require('../common');
const assert = require('assert');

process.env.NODE_DISABLE_COLORS = true;
process.stderr.columns = 20;

// Confirm that there is no position indicator.
assert.throws(
  () => { assert.deepStrictEqual('a'.repeat(30), 'a'.repeat(31)); },
  (err) => !err.message.includes('^')
);

// Confirm that there is a position indicator.
assert.throws(
  () => { assert.deepStrictEqual('aaa', 'aaaa'); },
  (err) => err.message.includes('^')
);
