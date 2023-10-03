'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const { strictEqual } = require('node:assert');
const { relative } = require('node:path');
const { run } = require('node:test');
const fixture = fixtures.path('test-runner', 'never_ending_sync.js');
const relativePath = relative(process.cwd(), fixture);
const stream = run({
  files: [relativePath],
  timeout: common.platformTimeout(100),
});

stream.on('test:fail', common.mustCall((result) => {
  strictEqual(result.name, relativePath);
  strictEqual(result.details.error.failureType, 'testTimeoutFailure');
  strictEqual(result.line, 1);
  strictEqual(result.column, 1);
  strictEqual(result.file, fixture);
}));
