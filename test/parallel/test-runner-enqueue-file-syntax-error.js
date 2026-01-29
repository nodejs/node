// Flags: --no-warnings
'use strict';
const common = require('../common');
const assert = require('node:assert');
const { run } = require('node:test');
const fixtures = require('../common/fixtures');

const testFile = fixtures.path('test-runner', 'syntax-error-test.mjs');
const stream = run({
  files: [testFile],
  isolation: 'none',
});

stream.on('test:enqueue', common.mustCall((data) => {
  assert.ok(data.file, 'test:enqueue event should have file field');
  assert.strictEqual(data.file, testFile);
}));

stream.on('test:fail', common.mustCall((data) => {
  assert.ok(data.details.error);
  assert.match(data.details.error.message, /SyntaxError/);
}));
