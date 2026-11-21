'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

// Loop iterations must not inflate function call counts.
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const fixtures = require('../common/fixtures');

const fixture = fixtures.path('test-runner', 'coverage-function-count.js');
const jsonReporter = fixtures.fileURL('test-runner/custom_reporters/coverage.mjs');

for (const isolation of ['process', 'none']) {
  for (const reporter of [jsonReporter, 'lcov']) {
    const { status, signal, stdout, stderr } = spawnSync(process.execPath, [
      '--test',
      '--experimental-test-coverage',
      `--test-isolation=${isolation}`,
      '--test-coverage-exclude=!test/**',
      `--test-coverage-include=${fixture}`,
      '--test-reporter', reporter,
      fixture,
    ], { encoding: 'utf8' });

    assert.strictEqual(status, 0, stderr);
    assert.strictEqual(signal, null);
    assert.strictEqual(stderr, '');

    if (reporter === 'lcov') {
      assert.match(stdout, /^FNDA:1,sum$/m);
    } else {
      const { summary: { files } } = JSON.parse(stdout);
      assert.strictEqual(files.length, 1);
      const [file] = files;
      assert.strictEqual(file.functions.find((fn) => fn.name === 'sum').count, 1);
      assert.strictEqual(file.lines.find((line) => line.line === 8).count, 10);
      assert(file.branches.some((branch) => branch.count === 10));
    }
  }
}
