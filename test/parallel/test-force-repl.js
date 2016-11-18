'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

// spawn a node child process in "interactive" mode (force the repl)
const cp = spawn(process.execPath, ['-i']);
var timeoutId = setTimeout(function() {
  common.fail('timeout!');
}, common.platformTimeout(5000)); // give node + the repl 5 seconds to start

cp.stdout.setEncoding('utf8');

cp.stdout.once('data', common.mustCall(function(b) {
  clearTimeout(timeoutId);
  assert.strictEqual(b, '> ');
  cp.kill();
}));
