'use strict';

const common = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const exec = require('child_process').exec;

const node = process.execPath;

// test both sets of arguments that check syntax
const syntaxArgs = [
  ['-c'],
  ['--check']
];

// Match on the name of the `Error` but not the message as it is different
// depending on the JavaScript engine.
const syntaxErrorRE = /^SyntaxError: \b/m;
const notFoundRE = /^Error: Cannot find module/m;

// test good syntax with and without shebang
[
  'syntax/good_syntax.js',
  'syntax/good_syntax',
  'syntax/good_syntax_shebang.js',
  'syntax/good_syntax_shebang',
  'syntax/illegal_if_not_wrapped.js'
].forEach(function(file) {
  file = fixtures.path(file);

  // loop each possible option, `-c` or `--check`
  syntaxArgs.forEach(function(args) {
    const _args = args.concat(file);

    const cmd = [node, ..._args].join(' ');
    exec(cmd, common.mustCall((err, stdout, stderr) => {
      assert.ifError(err);
      assert.strictEqual(stdout, '', 'stdout produced');
      assert.strictEqual(stderr, '', 'stderr produced');
    }));
  });
});

// test bad syntax with and without shebang
[
  'syntax/bad_syntax.js',
  'syntax/bad_syntax',
  'syntax/bad_syntax_shebang.js',
  'syntax/bad_syntax_shebang'
].forEach(function(file) {
  file = fixtures.path(file);

  // loop each possible option, `-c` or `--check`
  syntaxArgs.forEach(function(args) {
    const _args = args.concat(file);
    const cmd = [node, ..._args].join(' ');
    exec(cmd, common.mustCall((err, stdout, stderr) => {
      assert.strictEqual(err instanceof Error, true);
      assert.strictEqual(err.code, 1, `code === ${err.code}`);

      // no stdout should be produced
      assert.strictEqual(stdout, '', 'stdout produced');

      // stderr should have a syntax error message
      assert(syntaxErrorRE.test(stderr), 'stderr incorrect');

      // stderr should include the filename
      assert(stderr.startsWith(file), "stderr doesn't start with the filename");
    }));
  });
});

// test file not found
[
  'syntax/file_not_found.js',
  'syntax/file_not_found'
].forEach(function(file) {
  file = fixtures.path(file);

  // loop each possible option, `-c` or `--check`
  syntaxArgs.forEach(function(args) {
    const _args = args.concat(file);
    const cmd = [node, ..._args].join(' ');
    exec(cmd, common.mustCall((err, stdout, stderr) => {
      // no stdout should be produced
      assert.strictEqual(stdout, '', 'stdout produced');

      // stderr should have a module not found error message
      assert(notFoundRE.test(stderr), 'stderr incorrect');

      assert.strictEqual(err.code, 1, `code === ${err.code}`);
    }));
  });
});
