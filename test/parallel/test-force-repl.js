'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

// Spawn a node child process in interactive mode (enabling the REPL) and
// confirm the '> ' prompt is included in the output.
const cp = spawn(process.execPath, ['-i']);

cp.stdout.setEncoding('utf8');

cp.stdout.once('data', common.mustCall(function(b) {
  assert.strictEqual(b, '> ');
  cp.kill();
}));
