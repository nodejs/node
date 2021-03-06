'use strict';

const common = require('../common');
const assert = require('assert');
const { exec } = require('child_process');
const fixtures = require('../common/fixtures');

const node = process.execPath;

// Test both sets of arguments that check syntax
const syntaxArgs = [
  ['-c'],
  ['--check']
];

// Match on the name of the `Error` but not the message as it is different
// depending on the JavaScript engine.
const syntaxErrorRE = /^SyntaxError: \b/m;

// Test bad syntax with and without shebang
[
  'syntax/bad_syntax.js',
  'syntax/bad_syntax',
  'syntax/bad_syntax_shebang.js',
  'syntax/bad_syntax_shebang'
].forEach(function(file) {
  file = fixtures.path(file);

  // Loop each possible option, `-c` or `--check`
  syntaxArgs.forEach(function(args) {
    const _args = args.concat(file);
    const cmd = [node, ..._args].join(' ');
    exec(cmd, common.mustCall((err, stdout, stderr) => {
      assert.strictEqual(err instanceof Error, true);
      assert.strictEqual(err.code, 1,
                         `code ${err.code} !== 1 for error:\n\n${err}`);

      // No stdout should be produced
      assert.strictEqual(stdout, '');

      // Stderr should have a syntax error message
      assert(syntaxErrorRE.test(stderr), `${syntaxErrorRE} === ${stderr}`);

      // stderr should include the filename
      assert(stderr.startsWith(file), `${stderr} starts with ${file}`);
    }));
  });
});
