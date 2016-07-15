'use strict';
const common = require('../common');
var assert = require('assert');
var fork = require('child_process').fork;
var args = ['foo', 'bar'];

var n = fork(common.fixturesDir + '/child-process-spawn-node.js', args);
assert.deepStrictEqual(args, ['foo', 'bar']);

n.on('message', function(m) {
  console.log('PARENT got message:', m);
  assert.ok(m.foo);
});

// https://github.com/joyent/node/issues/2355 - JSON.stringify(undefined)
// returns "undefined" but JSON.parse() cannot parse that...
assert.throws(function() { n.send(undefined); }, TypeError);
assert.throws(function() { n.send(); }, TypeError);

n.send({ hello: 'world' });

n.on('exit', common.mustCall(function(c) {
  assert.strictEqual(c, 0);
}));
