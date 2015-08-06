'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isMaster) {
  cluster.fork();
  cluster.fork();
  cluster.disconnect(common.mustCall(function() {
    assert.deepEqual(Object.keys(cluster.workers), []);
  }));
}
