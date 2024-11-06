// Flags: --experimental-eventsource
'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');

test('EventSource is a function', () => {
  assert.strictEqual(typeof EventSource, 'function');
});
