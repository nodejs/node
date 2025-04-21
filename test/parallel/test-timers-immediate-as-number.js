'use strict';
const common = require('../common');
const { test } = require('node:test');

test('setImmediate return value is coerced to a number', (t) => {
  const immediateId = +setImmediate(common.mustNotCall());
  clearImmediate(immediateId);

  t.assert.strictEqual(typeof immediateId, 'number');
  t.assert.ok(!Number.isNaN(immediateId), 'setImmediate return value is NaN');
});

test('setImmediate return value is coerced to a string', (t) => {
  const immediateId = '' + setImmediate(common.mustNotCall());
  clearImmediate(immediateId);

  t.assert.strictEqual(typeof immediateId, 'string');
  t.assert.match(immediateId, /^\d+$/, 'setImmediate return value is not non-negative integer');
});

test('clearImmediate with a non-used immediate id', () => {
  // No-op when the immediate id is not used
  const immediateId = setImmediate(common.mustCall());

  clearImmediate(immediateId + 1);
  clearImmediate(`${immediateId + 1}`);

  // Obviusly invalid ids
  clearImmediate(Number.MAX_SAFE_INTEGER);
  clearImmediate('random-string');
});

test('clearImmediate with a numeric immediate id coerced to a string', () => {
  const immediateId = +setImmediate(common.mustNotCall());
  clearImmediate(immediateId.toString());
});

test('clearImmediate with a string immediate id coerced to a number', () => {
  const immediateId = '' + setImmediate(common.mustNotCall());
  clearImmediate(Number(immediateId));
});

test('two coerced immediate ids are equal if taken from the same immediate', (t) => {
  const immediate = setImmediate(common.mustCall());
  t.assert.equal('' + immediate, +immediate);
});
