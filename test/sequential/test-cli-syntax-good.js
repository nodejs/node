'use strict';

const common = require('../common');
const assert = require('assert');
const { exec } = require('child_process');
const fixtures = require('../common/fixtures');

// The execPath might contain chars that should be escaped in a shell context.
// On non-Windows, we can pass the path via the env; `"` is not a valid char on
// Windows, so we can simply pass the path.
const execNode = (flag, file, callback) => exec(
  `"${common.isWindows ? process.execPath : '$NODE'}" ${flag} "${common.isWindows ? file : '$FILE'}"`,
  common.isWindows ? undefined : { env: { ...process.env, NODE: process.execPath, FILE: file } },
  callback,
);

// Test both sets of arguments that check syntax
const syntaxArgs = [
  '-c',
  '--check',
];

// Test good syntax with and without shebang
[
  'syntax/good_syntax.js',
  'syntax/good_syntax',
  'syntax/good_syntax.mjs',
  'syntax/good_syntax_shebang.js',
  'syntax/good_syntax_shebang',
  'syntax/illegal_if_not_wrapped.js',
].forEach(function(file) {
  file = fixtures.path(file);

  // Loop each possible option, `-c` or `--check`
  syntaxArgs.forEach(function(flag) {
    execNode(flag, file, common.mustCall((err, stdout, stderr) => {
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
