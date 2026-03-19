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

test('assert.ok does not hang on long single-line source', () => {
  // Generate a synthetic minified file: many variable declarations on a
  // single line followed by a failing assert.ok call.
  let code = '';
  for (let i = 0; i < 100000; i++) {
    code += `var a${i}=function(){return ${i}};`;
  }
  code += "require('node:assert').ok(false)";

  const file = path.join(tmpdir.path, 'assert-minified-perf.js');
  fs.writeFileSync(file, code);

  // Run the file as a child process with a generous timeout.
  // Before the fix, this would hang for minutes. After the fix, it should
  // complete in well under 5 seconds even on slow CI machines.
  const result = spawnSync(process.execPath, [file], {
    timeout: 10_000,
    encoding: 'utf8',
  });

  // The process should exit with code 1 (assertion error), not be killed
  // by the timeout signal.
  assert.strictEqual(result.signal, null,
    'Process was killed by timeout - assert.ok hung on long line');
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /AssertionError/);
});

test('assert.ok error message is correct for long single-line source', () => {
  // A long line with a failing assert at the end should still produce
  // a meaningful error message with the expression.
  const padding = ';'.repeat(5000);
  const code = padding + "require('node:assert').ok(false)";

  const file = path.join(tmpdir.path, 'assert-long-line-msg.js');
  fs.writeFileSync(file, code);

  const result = spawnSync(process.execPath, [file], {
    timeout: 10_000,
    encoding: 'utf8',
  });

  assert.strictEqual(result.signal, null);
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /ok\(false\)/);
});
