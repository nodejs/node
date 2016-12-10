'use strict';
/*
 * The goal of this test is to cover the Workers' implementation of
 * Worker.prototype.destroy. Worker.prototype.destroy is called within
 * the worker's context: once when the worker is still connected to the
 * master, and another time when it's not connected to it, so that we cover
 * both code paths.
 */

const common = require('../common');
var cluster = require('cluster');
var worker1, worker2;

if (cluster.isMaster) {
  worker1 = cluster.fork();
  worker2 = cluster.fork();

  [worker1, worker2].forEach(function(worker) {
    worker.on('disconnect', common.mustCall(function() {}));
    worker.on('exit', common.mustCall(function() {}));
  });
} else {
  if (cluster.worker.id === 1) {
    // Call destroy when worker is disconnected
    cluster.worker.process.on('disconnect', function() {
      cluster.worker.destroy();
    });

    cluster.worker.disconnect();
  } else {
    // Call destroy when worker is not disconnected yet
    cluster.worker.destroy();
  }
}
