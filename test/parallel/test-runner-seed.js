// Flags: --test-random-seed=42
'use strict';

require('../common');
const assert = require('assert');
const test = require('node:test');

test('test-runner-seed', (context) => {
  assert.strictEqual(context.seed, 42);
});
