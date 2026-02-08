'use strict';
const common = require('../common');
const assert = require('node:assert');
const { run, test } = require('node:test');

if (!process.env.NODE_TEST_CONTEXT) {
  const stream = run({ files: [__filename] });

  stream.on('test:pass', common.mustNotCall());
  stream.on('test:fail', common.mustCall((event) => {
    assert.strictEqual(event.expectFailure, true);
    assert.strictEqual(event.details.error.code, 'ERR_TEST_FAILURE');
    assert.strictEqual(event.details.error.failureType, 'testCodeFailure');
    assert.strictEqual(event.details.error.cause, 'Test passed but was expected to fail');
  }, 1));
} else {
  test('passing test', { expectFailure: true }, () => {});
}
