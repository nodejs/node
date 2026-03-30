'use strict';

const common = require('../common');
const { startNewREPLServer } = require('../common/repl');
const assert = require('node:assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Tests that an appropriate error is displayed if .load is called without a filename

const { replServer, input, output } = startNewREPLServer({ terminal: false });

output.write = common.mustCall(function(data) {
  assert.strictEqual(data, 'The "file" argument must be specified\n');
  output.write = () => {};
});

input.run(['.load']);

replServer.close();
