'use strict';
const common = require('../common');
const assert = require('node:assert');
const fixtures = require('../common/fixtures');
const { run } = require('node:test');

const testFile = fixtures.path('test-runner', 'throws-after-test.mjs');
const testRun = run({
  files: [testFile],
  isolation: 'none'
});

testRun.on('test:pass', common.mustCall((test) => {
  assert.strictEqual(test.name, 'registered before the error');
}));

testRun.on('test:fail', common.mustCall((test) => {
  assert.strictEqual(test.name, testFile);
  assert.match(test.details.error.toString(), /TypeError: Cannot read properties of null/);
}));
