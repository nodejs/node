'use strict';
require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

function removeDuration(str) {
  return str.replace(/duration_ms:? .+/g, 'duration_ms: ');
}

{
  // sanity
  const child = spawnSync(process.execPath, [fixtures.path('test-runner/index.test.js')]);
  const stdout = removeDuration(child.stdout.toString());
  const expected = fixtures.readSync('test-runner/index.test.tap').toString();
  
  assert.strictEqual(stdout, expected);
}
{
  // nested tests
  const child = spawnSync(process.execPath, [fixtures.path('test-runner/nested.test.js')]);
  const stdout = removeDuration(child.stdout.toString());
  const expected = fixtures.readSync('test-runner/nested.test.tap').toString();
  
  assert.strictEqual(stdout, expected);
}
{
  // using test runner cli
  const file = fixtures.path('test-runner/nested.test.js');
  const child = spawnSync(process.execPath, ['--test', fixtures.path('test-runner/nested.test.js')]);
  const stdout = removeDuration(child.stdout.toString());
  const expected = fixtures.readSync('test-runner/nested.test.cli.tap').toString().replace(/\$FILE_PATH/g, file);
  
  assert.strictEqual(stdout, expected);
}