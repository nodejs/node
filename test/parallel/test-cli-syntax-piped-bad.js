'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

const node = process.execPath;

// Test both sets of arguments that check syntax
const syntaxArgs = [
  '-c',
  '--check',
];

// Match on the name of the `Error` but not the message as it is different
// depending on the JavaScript engine.
const syntaxErrorRE = /^SyntaxError: Unexpected identifier\b/m;

// Should throw if code piped from stdin with --check has bad syntax
// loop each possible option, `-c` or `--check`
syntaxArgs.forEach(function(arg) {
  const stdin = 'var foo bar;';
  const c = spawnSync(node, [arg], { encoding: 'utf8', input: stdin });

  // stderr should include '[stdin]' as the filename
  assert(c.stderr.startsWith('[stdin]'), `${c.stderr} starts with ${stdin}`);

  // No stdout should be produced
  assert.strictEqual(c.stdout, '');

  // stderr should have a syntax error message
  assert.match(c.stderr, syntaxErrorRE);

  assert.strictEqual(c.status, 1);
});

// Check --input-type=module
syntaxArgs.forEach(function(arg) {
  const stdin = 'export var p = 5; var foo bar;';
  const c = spawnSync(
    node,
    ['--input-type=module', '--no-warnings', arg],
    { encoding: 'utf8', input: stdin }
  );

  // stderr should include '[stdin]' as the filename
  assert(c.stderr.startsWith('[stdin]'), `${c.stderr} starts with ${stdin}`);

  // No stdout should be produced
  assert.strictEqual(c.stdout, '');

  // stderr should have a syntax error message
  assert.match(c.stderr, syntaxErrorRE);

  assert.strictEqual(c.status, 1);
});
