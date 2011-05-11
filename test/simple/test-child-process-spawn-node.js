var assert = require('assert');
var common = require('../common');
var fork = require('child_process').fork;

var n = fork(common.fixturesDir + '/child-process-spawn-node.js');

var messageCount = 0;

n.on('message', function(m) {
  console.log('PARENT got message:', m);
  assert.ok(m.foo);
  messageCount++;
});

n.send({ hello: 'world' });

var childExitCode = -1;
n.on('exit', function(c) {
  childExitCode = c;
});

process.on('exit', function() {
  assert.ok(childExitCode == 0);
});
