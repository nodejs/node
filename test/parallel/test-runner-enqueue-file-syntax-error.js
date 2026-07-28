'use strict';
const common = require('../common');
const assert = require('node:assert');
const fixtures = require('../common/fixtures');
const { run } = require('node:test');

const testFile = fixtures.path('test-runner', 'syntax-error-test.mjs');
const testRun = run({
  files: [testFile],
  isolation: 'none'
});

testRun.on('test:enqueue', common.mustCall((test) => {
  assert.strictEqual(test.file, testFile);
}));

testRun.on('test:fail', common.mustCall((test) => {
  assert.match(test.details.error.toString(), /SyntaxError: Unexpected token 'true'/);
}));
