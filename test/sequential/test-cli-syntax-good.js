'use strict';

const common = require('../common');
const assert = require('assert');
const { exec } = require('child_process');
const fixtures = require('../common/fixtures');

const node = process.execPath;

// test both sets of arguments that check syntax
const syntaxArgs = [
  ['-c'],
  ['--check']
];

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
      if (err) {
        console.log('-- stdout --');
        console.log(stdout);
        console.log('-- stderr --');
        console.log(stderr);
      }
      assert.ifError(err);
      assert.strictEqual(stdout, '');
      assert.strictEqual(stderr, '');
    }));
  });
});
