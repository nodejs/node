'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');

const assert = require('node:assert');
const repl = require('node:repl');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Tests that an appropriate error is displayed if .save is called without a filename

const input = new ArrayStream();
const output = new ArrayStream();

const replServer = repl.start({
  prompt: '',
  input,
  output,
  allowBlockingCompletions: true,
});

// Some errors are passed to the domain, but do not callback
replServer._domain.on('error', assert.ifError);

output.write = common.mustCall(function(data) {
  assert.strictEqual(data, 'The "file" argument must be specified\n');
  output.write = () => {};
});

input.run(['.save']);

replServer.close();
