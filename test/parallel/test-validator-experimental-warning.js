'use strict';

const common = require('../common');
const assert = require('assert');
const { test } = require('node:test');

common.expectWarning(
  'ExperimentalWarning',
  'validator is an experimental feature and might change at any time',
);

const validator = require('node:validator');

test('Schema is usable after the experimental warning fires', () => {
  const schema = new validator.Schema({ type: 'string', minLength: 1 });
  assert.strictEqual(schema.validate('ok').valid, true);
  const bad = schema.validate('');
  assert.strictEqual(bad.valid, false);
  assert.strictEqual(bad.errors[0].code, 'STRING_TOO_SHORT');
});

test('codes export is accessible and populated', () => {
  assert.strictEqual(typeof validator.codes, 'object');
  assert.strictEqual(validator.codes.INVALID_TYPE, 'INVALID_TYPE');
  assert.strictEqual(validator.codes.STRING_TOO_SHORT, 'STRING_TOO_SHORT');
});
