'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { relative } = require('node:path');
const { run } = require('node:test');
const fixture = fixtures.path('test-runner', 'index.js');
const relativePath = relative(process.cwd(), fixture);
const stream = run({
  files: [relativePath],
  timeout: common.platformTimeout(100),
});

stream.on('test:fail', common.mustCall((result) => {
  assert.strictEqual(result.name, relativePath);
  assert.strictEqual(result.details.error.failureType, 'testCodeFailure');
  assert.strictEqual(result.line, 1);
  assert.strictEqual(result.column, 1);
  assert.strictEqual(result.file, fixture);
}));
