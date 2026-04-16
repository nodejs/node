'use strict';

require('../common');
const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');

[
  '{using x=null, []=null;}',
  '{using x=null, {}=null;}',
  'async function f() { { await using x=null, []=null; } }',
  'async function f() { { await using x=null, {}=null; } }',
].forEach((script) => {
  spawnSyncAndAssert(process.execPath, ['--eval', script], {
    status: 1,
    signal: null,
    stderr(output) {
      assert.match(output, /SyntaxError/);
      assert.doesNotMatch(output, /Assertion failed/);
      assert.doesNotMatch(output, /Native stack trace/);
    },
    stdout: '',
  });
});
