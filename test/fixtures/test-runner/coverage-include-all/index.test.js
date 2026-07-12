'use strict';

const assert = require('node:assert');
const test = require('node:test');
const covered = require('./covered');

test('covered source is executed', () => {
  assert.strictEqual(covered(), 'covered');
});
