'use strict';
const common = require('../common');
const cluster = require('cluster');
const assert = require('assert');
const numCPUs = require('os').cpus().length;

common.skipIfInspectorDisabled();

checkForInspectSupport('--inspect');

function checkForInspectSupport(flag) {

  const nodeOptions = JSON.stringify(flag);
  process.env.NODE_OPTIONS = flag;

  if (cluster.isMaster) {
    for (let i = 0; i < numCPUs; i++) {
      cluster.fork();
    }

    cluster.on('online', (worker) => {
      worker.disconnect();
    });

    cluster.on('exit', (worker, code, signal) => {
      if (worker.exitedAfterDisconnect === false) {
        assert.fail(`For ${nodeOptions}, failed to start cluster\
 with inspect option`);
      }
    });
  }
}
