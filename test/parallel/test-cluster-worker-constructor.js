'use strict';
// test-cluster-worker-constructor.js
// validates correct behavior of the cluster.Worker constructor

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
var worker;

worker = new cluster.Worker();
assert.equal(worker.suicide, undefined);
assert.equal(worker.state, 'none');
assert.equal(worker.id, 0);
assert.equal(worker.process, undefined);

worker = new cluster.Worker({
  id: 3,
  state: 'online',
  process: process
});
assert.equal(worker.suicide, undefined);
assert.equal(worker.state, 'online');
assert.equal(worker.id, 3);
assert.equal(worker.process, process);

worker = cluster.Worker.call({}, {id: 5});
assert(worker instanceof cluster.Worker);
assert.equal(worker.id, 5);
