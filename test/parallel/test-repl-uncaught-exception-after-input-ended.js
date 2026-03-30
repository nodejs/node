'use strict';
const common = require('../common');
const { start } = require('node:repl');
const { PassThrough } = require('node:stream');
const assert = require('node:assert');

// This test verifies that uncaught exceptions in the REPL
// do not bring down the process, even if stdin may already
// have been ended at that point (and the REPL closed as
// a result of that).
const input = new PassThrough();
const output = new PassThrough().setEncoding('utf8');
start({
  input,
  output,
  terminal: false,
});

input.end('setImmediate(() => { throw new Error("test"); });\n');

setImmediate(common.mustCall(() => {
  assert.match(output.read(), /Uncaught Error: test/);
}));
