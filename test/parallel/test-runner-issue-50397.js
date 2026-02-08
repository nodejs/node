'use strict';

// Regression test for https://github.com/nodejs/node/issues/50397:
// ensure --test preserves AssertionError actual type across isolation modes.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('node:assert');
const fixtures = require('../common/fixtures');

for (const isolation of ['none', 'process']) {
  const args = [
    '--test',
    '--test-reporter=spec',
    `--test-isolation=${isolation}`,
    fixtures.path('test-runner/issue-50397/prototype-mismatch.js'),
  ];
  spawnSyncAndAssert(process.execPath, args, {
    status: 1,
    signal: null,
    stderr: '',
    stdout(output) {
      // Spec reporter output varies between inspect forms; accept both while
      // still requiring the restored constructor name.
      assert.match(output, /actual:\s+(?:\[ExtendedArray\]|ExtendedArray\(1\)\s+\[\s*'hello'\s*\])/);
      assert.doesNotMatch(output, /actual:\s+\[Array\]/);
    },
  });
}
