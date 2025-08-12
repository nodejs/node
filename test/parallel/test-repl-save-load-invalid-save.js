'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');

const assert = require('node:assert');
const repl = require('node:repl');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Test for the appropriate handling of cases in which REPL saves fail

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

// NUL (\0) is disallowed in filenames in UNIX-like operating systems and
// Windows so we can use that to test failed saves.
const invalidFilePath = tmpdir.resolve('\0\0\0\0\0');

output.write = common.mustCall(function(data) {
  assert.strictEqual(data, `Failed to save: ${invalidFilePath}\n`);
  output.write = () => {};
});

input.run([`.save ${invalidFilePath}`]);

replServer.close();
