'use strict';
var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var fork = cluster.fork;

if (cluster.isMaster) {
  fork();
  fork();
  cluster.disconnect(common.mustCall(function() {
    assert.deepEqual(Object.keys(cluster.workers), []);
  }));
}
