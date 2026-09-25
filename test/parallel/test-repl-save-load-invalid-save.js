'use strict';

const common = require('../common');
const assert = require('node:assert');
const { startNewREPLServer } = require('../common/repl');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Test for the appropriate handling of cases in which REPL saves fail

const { replServer, input, output } = startNewREPLServer({ terminal: false });

// NUL (\0) is disallowed in filenames in UNIX-like operating systems and
// Windows so we can use that to test failed saves.
const invalidFilePath = tmpdir.resolve('\0\0\0\0\0');

output.write = common.mustCall(function(data) {
  assert.strictEqual(data, `Failed to save: ${invalidFilePath}\n`);
  output.write = () => {};
});

input.run([`.save ${invalidFilePath}`]);

replServer.close();
