'use strict';

const common = require('../common');
const { startNewREPLServer } = require('../common/repl');
const assert = require('node:assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Tests that an appropriate error is displayed if the user tries to load a directory instead of a file

const { replServer, input, output } = startNewREPLServer({ terminal: false });

const dirPath = tmpdir.path;

output.write = common.mustCall(function(data) {
  assert.strictEqual(data, `Failed to load: ${dirPath} is not a valid file\n`);
  output.write = () => {};
});

input.run([`.load ${dirPath}`]);

replServer.close();
