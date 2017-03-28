'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

// Check that inspector can run on an ephemeral port.

const assert = require('assert');
const exec = require('child_process').exec;

const printA = require.resolve('../fixtures/printA.js');
const cmd = [
  process.execPath,
  '--inspect=0',
  printA,
].join(' ');

exec(cmd, common.mustCall(function(err, _, stderr) {
  assert.ifError(err);
  const port1 = Number(stderr.match(/Debugger listening on port (\d+)/)[1]);
  const port2 = Number(stderr.match(/chrome-devtools.*:(\d+)/)[1]);
  assert.strictEqual(port1, port2);
  assert(port1 > 0);
}));
