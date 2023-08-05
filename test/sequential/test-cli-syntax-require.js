'use strict';

const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');
const fixtures = require('../common/fixtures');

const node = process.execPath;

// Match on the name of the `Error` but not the message as it is different
// depending on the JavaScript engine.
const syntaxErrorRE = /^SyntaxError: \b/m;

// Should work with -r flags
['-c', '--check'].forEach(function(checkFlag) {
  ['-r', '--require'].forEach(function(requireFlag) {
    const preloadFile = fixtures.path('no-wrapper.js');
    const file = fixtures.path('syntax', 'illegal_if_not_wrapped.js');
    const args = [requireFlag, preloadFile, checkFlag, file];
    const cp = spawn(node, args);

    // No stdout should be produced
    cp.stdout.on('data', common.mustNotCall('stdout data event'));

    const stderrBuffer = [];
    cp.stderr.on('data', (chunk) => stderrBuffer.push(chunk));

    cp.on('exit', common.mustCall((code, signal) => {
      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);

      const stderr = Buffer.concat(stderrBuffer).toString('utf-8');
      // stderr should have a syntax error message
      assert.match(stderr, syntaxErrorRE);

      // stderr should include the filename
      assert(stderr.startsWith(file), `${stderr} starts with ${file}`);
    }));

  });
});
