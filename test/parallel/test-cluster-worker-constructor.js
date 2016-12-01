'use strict';
// test-cluster-worker-constructor.js
// validates correct behavior of the cluster.Worker constructor

require('../common');
var assert = require('assert');
var cluster = require('cluster');
var worker;

worker = new cluster.Worker();
assert.strictEqual(worker.suicide, undefined);
assert.strictEqual(worker.state, 'none');
assert.strictEqual(worker.id, 0);
assert.strictEqual(worker.process, undefined);

worker = new cluster.Worker({
  id: 3,
  state: 'online',
  process: process
});
assert.strictEqual(worker.suicide, undefined);
assert.strictEqual(worker.state, 'online');
assert.strictEqual(worker.id, 3);
assert.strictEqual(worker.process, process);

worker = cluster.Worker.call({}, {id: 5});
assert(worker instanceof cluster.Worker);
assert.strictEqual(worker.id, 5);
