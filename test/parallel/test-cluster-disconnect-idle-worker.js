'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const fork = cluster.fork;

if (cluster.isMaster) {
  fork(); // it is intentionally called `fork` instead of
  fork(); // `cluster.fork` to test that `this` is not used
  cluster.disconnect(common.mustCall(() => {
    assert.deepStrictEqual(Object.keys(cluster.workers), []);
  }));
}
