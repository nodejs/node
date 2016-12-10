'use strict';
require('../common');
var assert = require('assert');
var cluster = require('cluster');

assert(cluster.isMaster);

// The cluster.settings object is cloned even though the current implementation
// makes that unecessary. This is to make the test less fragile if the
// implementation ever changes such that cluster.settings is mutated instead of
// replaced.
function cheapClone(obj) {
  return JSON.parse(JSON.stringify(obj));
}

var configs = [];

// Capture changes
cluster.on('setup', function() {
  console.log('"setup" emitted', cluster.settings);
  configs.push(cheapClone(cluster.settings));
});

var execs = [
  'node-next',
  'node-next-2',
  'node-next-3',
];

process.on('exit', function assertTests() {
  // Tests that "setup" is emitted for every call to setupMaster
  assert.strictEqual(configs.length, execs.length);

  assert.strictEqual(configs[0].exec, execs[0]);
  assert.strictEqual(configs[1].exec, execs[1]);
  assert.strictEqual(configs[2].exec, execs[2]);
});

// Make changes to cluster settings
execs.forEach(function(v, i) {
  setTimeout(function() {
    cluster.setupMaster({ exec: v });
  }, i * 100);
});

// cluster emits 'setup' asynchronously, so we must stay alive long
// enough for that to happen
setTimeout(function() {
  console.log('cluster setup complete');
}, (execs.length + 1) * 100);
