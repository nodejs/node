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

// Match on the name of the `Error` but not the message as it is different
// depending on the JavaScript engine.
const syntaxErrorRE = /^SyntaxError: \b/m;

// Should throw if code piped from stdin with --check has bad syntax
// loop each possible option, `-c` or `--check`
syntaxArgs.forEach(function(args) {
  const stdin = 'var foo bar;';
  const c = spawnSync(node, args, { encoding: 'utf8', input: stdin });

  // stderr should include '[stdin]' as the filename
  assert(c.stderr.startsWith('[stdin]'), `${c.stderr} starts with ${stdin}`);

  // no stdout or stderr should be produced
  assert.strictEqual(c.stdout, '');

  // stderr should have a syntax error message
  assert(syntaxErrorRE.test(c.stderr), `${syntaxErrorRE} === ${c.stderr}`);

  assert.strictEqual(c.status, 1);
});
