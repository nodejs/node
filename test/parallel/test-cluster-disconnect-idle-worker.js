'use strict';
var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');

if (cluster.isMaster) {
  cluster.fork();
  cluster.fork();
  cluster.disconnect(common.mustCall(function() {
    assert.deepEqual(Object.keys(cluster.workers), []);
  }));
}
