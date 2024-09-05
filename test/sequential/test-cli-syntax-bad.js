'use strict';

require('../common');
const { exec } = require('child_process');
const { test } = require('node:test');
const fixtures = require('../common/fixtures');

const node = process.execPath;

// Test both sets of arguments that check syntax
const syntaxArgs = [
  ['-c'],
  ['--check'],
];

// Match on the name of the `Error` but not the message as it is different
// depending on the JavaScript engine.
const syntaxErrorRE = /^SyntaxError: \b/m;

// Test bad syntax with and without shebang
[
  'syntax/bad_syntax.js',
  'syntax/bad_syntax',
  'syntax/bad_syntax_shebang.js',
  'syntax/bad_syntax_shebang',
].forEach((file) => {
  const path = fixtures.path(file);

  // Loop each possible option, `-c` or `--check`
  syntaxArgs.forEach((args) => {
    test(`Checking syntax for ${file} with ${args.join(' ')}`, async (t) => {
      const _args = args.concat(path);
      const cmd = [node, ..._args].join(' ');

      try {
        const { stdout, stderr } = await execPromise(cmd);

        // No stdout should be produced
        t.assert.strictEqual(stdout, '');

        // Stderr should have a syntax error message
        t.assert.match(stderr, syntaxErrorRE);

        // stderr should include the filename
        t.assert.ok(stderr.startsWith(path));
      } catch (err) {
        t.assert.strictEqual(err.code, 1);
      }
    });
  });
});

// Helper function to promisify exec
function execPromise(cmd) {
  const { promise, resolve, reject } = Promise.withResolvers();
  exec(cmd, (err, stdout, stderr) => {
    if (err) return reject({ ...err, stdout, stderr });
    resolve({ stdout, stderr });
  });
  return promise;
}
