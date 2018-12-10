'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

const node = process.execPath;

// test both sets of arguments that check syntax
const syntaxArgs = [
  ['-c'],
  ['--check']
];

// Should not execute code piped from stdin with --check.
// Loop each possible option, `-c` or `--check`.
syntaxArgs.forEach(function(args) {
  const stdin = 'throw new Error("should not get run");';
  const c = spawnSync(node, args, { encoding: 'utf8', input: stdin });

  // no stdout or stderr should be produced
  assert.strictEqual(c.stdout, '');
  assert.strictEqual(c.stderr, '');

  assert.strictEqual(c.status, 0);
});
