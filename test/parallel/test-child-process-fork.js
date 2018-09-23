'use strict';
var assert = require('assert');
var common = require('../common');
var fork = require('child_process').fork;
var args = ['foo', 'bar'];

var n = fork(common.fixturesDir + '/child-process-spawn-node.js', args);
assert.deepEqual(args, ['foo', 'bar']);

var messageCount = 0;

n.on('message', function(m) {
  console.log('PARENT got message:', m);
  assert.ok(m.foo);
  messageCount++;
});

// https://github.com/joyent/node/issues/2355 - JSON.stringify(undefined)
// returns "undefined" but JSON.parse() cannot parse that...
assert.throws(function() { n.send(undefined); }, TypeError);
assert.throws(function() { n.send(); }, TypeError);

n.send({ hello: 'world' });

var childExitCode = -1;
n.on('exit', function(c) {
  childExitCode = c;
});

process.on('exit', function() {
  assert.ok(childExitCode == 0);
});
