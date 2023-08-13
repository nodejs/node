'use strict';
const common = require('../../../common');
const assert = require('assert');
const test = require('node:test');

test('keep error', (t) => {
  assert.strictEqual(0, 1);
});

test('dont show', common.mustNotCall((t) => {
  assert.strictEqual(0, 2);
}));
