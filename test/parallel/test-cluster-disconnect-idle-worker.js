'use strict';
var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var fork = cluster.fork;

if (cluster.isMaster) {
  fork(); // it is intentionally called `fork` instead of
  fork(); // `cluster.fork` to test that `this` is not used
  cluster.disconnect(common.mustCall(function() {
    assert.deepStrictEqual(Object.keys(cluster.workers), []);
  }));
}
