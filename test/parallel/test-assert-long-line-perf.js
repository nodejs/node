'use strict';

// Verify that assert.ok with minified/bundled single-line code does not hang.
// Regression test for https://github.com/nodejs/node/issues/52677

require('../common');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const { test } = require('node:test');

test('assert.ok completes quickly on long single-line source', () => {
  // Generate a single-line file that exceeds kMaxSourceLineLength (2048)
  // with semicolons as statement boundaries, followed by a failing assert.
  // ~3000 chars is enough to trigger windowing without stressing the filesystem.
  const padding = 'var x=1;'.repeat(375);
  const code = padding + "require('node:assert').ok(false)";

  const file = path.join(tmpdir.path, 'assert-minified-perf.js');
  fs.writeFileSync(file, code);

  const start = process.hrtime.bigint();
  const result = spawnSync(process.execPath, [file], {
    timeout: 30_000,
    encoding: 'utf8',
  });
  const elapsedMs = Number(process.hrtime.bigint() - start) / 1e6;

  // The process should exit with code 1 (assertion error), not be killed
  // by the timeout signal.
  assert.strictEqual(result.signal, null,
    'Process was killed by timeout - assert.ok hung on long line');
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /AssertionError/);

  // With windowing, this should complete in well under 10 seconds even on
  // slow CI machines. Without windowing, long lines can take minutes.
  assert.ok(elapsedMs < 10_000,
    `Expected completion in <10s, took ${elapsedMs.toFixed(0)}ms`);
});

test('assert.ok error message is correct for long single-line source', () => {
  // A long line with semicolons followed by a failing assert. The windowing
  // logic should find the semicolons and extract the expression correctly.
  const padding = ';'.repeat(3000);
  const code = padding + "require('node:assert').ok(false)";

  const file = path.join(tmpdir.path, 'assert-long-line-msg.js');
  fs.writeFileSync(file, code);

  const result = spawnSync(process.execPath, [file], {
    timeout: 30_000,
    encoding: 'utf8',
  });

  assert.strictEqual(result.signal, null);
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /ok\(false\)/);
});
