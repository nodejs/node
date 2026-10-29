'use strict';

const common = require('../common');
const { startNewREPLServer } = require('../common/repl');
const assert = require('node:assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Tests that an appropriate error is displayed if the user tries to load a non existent file

const { replServer, input, output } = startNewREPLServer({ terminal: false });

const filePath = tmpdir.resolve('file.does.not.exist');

output.write = common.mustCall(function(data) {
  assert.strictEqual(data, `Failed to load: ${filePath}\n`);
  output.write = () => {};
});

input.run([`.load ${filePath}`]);

replServer.close();
