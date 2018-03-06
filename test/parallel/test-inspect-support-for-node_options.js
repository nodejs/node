'use strict';
const common = require('../common');
const cluster = require('cluster');
const assert = require('assert');

if (process.config.variables.node_without_node_options)
  common.skip('missing NODE_OPTIONS support');


checkForInspectSupport('--inspect');

function checkForInspectSupport(flag) {
  const env = Object.assign({}, process.env, { NODE_OPTIONS: flag });
  const o = JSON.stringify(flag);

  if (cluster.isMaster) {
    cluster.fork(env);

    cluster.on('online', (worker) => {
      process.exit(0);
    });

    cluster.on('exit', (worker, code, signal) => {
      assert.fail(`For ${o}, failed to start a cluster with inspect option`);
    });
  }
}
