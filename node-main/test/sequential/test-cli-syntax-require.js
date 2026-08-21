'use strict';

require('../common');
const assert = require('assert');
const { spawnSyncAndExit } = require('../common/child_process');
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
    spawnSyncAndExit(node, args, {
      status: 1,
      signal: null,
      trim: true,
      stdout: '',
      stderr(output) {
        // stderr should have a syntax error message
        assert.match(output, syntaxErrorRE);

        // stderr should include the filename
        assert(output.startsWith(file), `${output} starts with ${file}`);
      }
    });
  });
});
