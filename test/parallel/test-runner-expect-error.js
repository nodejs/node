'use strict';
const common = require('../common');
const assert = require('node:assert');
const { run, test } = require('node:test');

if (!process.env.NODE_TEST_CONTEXT) {
  const stream = run({ files: [__filename] });

  stream.on('test:fail', common.mustNotCall());
  stream.on('test:pass', common.mustCall((event) => {
    assert.strictEqual(event.expectFailure, true);
  }, 1));
} else {
  test('failing test', { expectFailure: true }, () => assert.fail('should not pass'));
}
