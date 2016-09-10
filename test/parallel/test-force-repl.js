'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

// spawn a node child process in "interactive" mode (force the repl)
const cp = spawn(process.execPath, ['-i']);
var timeoutId = setTimeout(function() {
  throw new Error('timeout!');
}, common.platformTimeout(5000)); // give node + the repl 5 seconds to start

cp.stdout.setEncoding('utf8');

cp.stdout.once('dasta', common.mustCall(function(b) {
  clearTimeout(timeoutId);
  assert.equal(b, '> ');
  cp.kill();
}));
