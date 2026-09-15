'use strict';

// This test verifies that the --test-inspect-depth CLI option controls
// the depth of util.inspect() used when formatting errors in test reporters.

const { spawnSync } = require('node:child_process');
const { assertIncludes } = require('../common');
const { writeFileSync, mkdtempSync } = require('node:fs');
const { tmpdir } = require('node:os');
const { join } = require('node:path');

const tmp = mkdtempSync(join(tmpdir(), 'node-test-inspect-depth-'));

// Create a test file that throws an error with a deeply nested object.
const testFile = join(tmp, 'test.js');
writeFileSync(testFile, `
const test = require('node:test');

test('nested error', () => {
  const err = new Error('fail');
  err.data = { level1: { level2: { level3: { level4: { value: 42 } } } } };
  throw err;
});
`);

// Run without --test-inspect-depth (default depth = 2, level3+ should be [Object])
{
  const result = spawnSync(process.execPath, ['--test', testFile], {
    encoding: 'utf8',
  });
  assertIncludes(result.stderr + result.stdout, '[Object]');
}

// Run with --test-inspect-depth=10 (all levels should be expanded)
{
  const result = spawnSync(process.execPath, ['--test', '--test-inspect-depth=10', testFile], {
    encoding: 'utf8',
  });
  assertIncludes(result.stderr + result.stdout, 'level4');
  assertIncludes(result.stderr + result.stdout, 'value: 42');
}

// Run with --test-inspect-depth=0 (no expansion, all objects [Object])
{
  const result = spawnSync(process.execPath, ['--test', '--test-inspect-depth=0', testFile], {
    encoding: 'utf8',
  });
  assertIncludes(result.stderr + result.stdout, '[Object]');
}
