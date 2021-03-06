'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

const node = process.execPath;

// Test both sets of arguments that check syntax
const syntaxArgs = [
  '-c',
  '--check'
];

// Should not execute code piped from stdin with --check.
// Loop each possible option, `-c` or `--check`.
syntaxArgs.forEach(function(arg) {
  const stdin = 'throw new Error("should not get run");';
  const c = spawnSync(node, [arg], { encoding: 'utf8', input: stdin });

  // No stdout or stderr should be produced
  assert.strictEqual(c.stdout, '');
  assert.strictEqual(c.stderr, '');

  assert.strictEqual(c.status, 0);
});

// Check --input-type=module
syntaxArgs.forEach(function(arg) {
  const stdin = 'export var p = 5; throw new Error("should not get run");';
  const c = spawnSync(
    node,
    ['--no-warnings', '--input-type=module', arg],
    { encoding: 'utf8', input: stdin }
  );

  // No stdout or stderr should be produced
  assert.strictEqual(c.stdout, '');
  assert.strictEqual(c.stderr, '');

  assert.strictEqual(c.status, 0);
});
