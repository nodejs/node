'use strict';

require('../common');
const { test } = require('node:test');
const assert = require('node:assert');
const { constants } = require('node:os');


test('Verify that signals constant is immutable', () => {
  assert.throws(() => constants.signals.FOOBAR = 1337, TypeError);
});
