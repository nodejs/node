'use strict';

// Regression test for https://github.com/nodejs/node/issues/63169
// When --enable-source-maps is enabled but the script has no source map,
// assert(false) should throw AssertionError, not TypeError ERR_INVALID_ARG_TYPE.

require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');
const fixtures = require('../common/fixtures');

function runWithSourceMaps(fixture) {
  return spawnSync(process.execPath, [
    '--enable-source-maps',
    fixtures.path('source-map', fixture),
  ]);
}

test('assert(false) in .mjs with --enable-source-maps throws AssertionError', () => {
  const result = runWithSourceMaps('assert-no-source-map.mjs');

  assert.strictEqual(result.status, 0, `process exited with ${result.status}: ${result.stderr}`);
  const lines = result.stdout.toString().trim().split('\n');
  assert.strictEqual(lines[0], 'AssertionError');
  assert.strictEqual(lines[1], 'ERR_ASSERTION');
  assert.match(lines[2], /assert\(false\)/);
});

test('assert.ok(false) in .cjs with --enable-source-maps throws AssertionError', () => {
  const result = runWithSourceMaps('assert-no-source-map.cjs');

  assert.strictEqual(result.status, 0, `process exited with ${result.status}: ${result.stderr}`);
  const lines = result.stdout.toString().trim().split('\n');
  assert.strictEqual(lines[0], 'AssertionError');
  assert.strictEqual(lines[1], 'ERR_ASSERTION');
  assert.match(lines[2], /assert\.ok\(false\)/);
});
