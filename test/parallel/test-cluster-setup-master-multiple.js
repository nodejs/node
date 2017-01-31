'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

assert(cluster.isMaster);

// The cluster.settings object is cloned even though the current implementation
// makes that unecessary. This is to make the test less fragile if the
// implementation ever changes such that cluster.settings is mutated instead of
// replaced.
function cheapClone(obj) {
  return JSON.parse(JSON.stringify(obj));
}

const configs = [];
const execs = [
  'node-next',
  'node-next-2',
  'node-next-3',
];
const calls = execs.length;

// Capture changes
cluster.on('setup', common.mustCall(() => {
  configs.push(cheapClone(cluster.settings));
}, calls));


process.on('exit', function assertTests() {
  // Tests that "setup" is emitted for every call to setupMaster
  assert.strictEqual(configs.length, execs.length);

  for (let i = 0; i < calls; i++) {
    assert.strictEqual(configs[i].exec, execs[i]);
  }
});

// Make changes to cluster settings
execs.forEach((v, i) => {
  setTimeout(() => {
    cluster.setupMaster({ exec: v });
  }, i * 100);
});

// cluster emits 'setup' asynchronously, so we must stay alive long
// enough for that to happen
setTimeout(() => {}, (execs.length + 1) * 100);
