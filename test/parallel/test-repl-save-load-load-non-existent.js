'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');

const assert = require('node:assert');
const repl = require('node:repl');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Tests that an appropriate error is displayed if the user tries to load a non existent file

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

const filePath = tmpdir.resolve('file.does.not.exist');

output.write = common.mustCall(function(data) {
  assert.strictEqual(data, `Failed to load: ${filePath}\n`);
  output.write = () => {};
});

input.run([`.load ${filePath}`]);

replServer.close();
