var assert = require('assert');
var spawn = require('child_process').spawn;
var common = require('../common');

var sub = common.fixturesDir + '/child-process-channel.js';

var child = spawn(process.execPath, [ sub ], {
  customFds: [0, 1, 2],
  wantChannel: true
});

console.log("fds", child.fds);

assert.ok(child.fds.length == 4);
assert.ok(child.fds[3] >= 0);

var childExitCode = -1;

child.on('exit', function(code) {
  childExitCode = code;
});

process.on('exit', function() {
  assert.ok(childExitCode == 0);
});
