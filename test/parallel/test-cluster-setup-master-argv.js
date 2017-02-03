'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

setTimeout(common.mustNotCall('setup not emitted'), 1000).unref();

cluster.on('setup', common.mustCall(function() {
  const clusterArgs = cluster.settings.args;
  const realArgs = process.argv;
  assert.strictEqual(clusterArgs[clusterArgs.length - 1],
                     realArgs[realArgs.length - 1]);
}));

assert.notStrictEqual(process.argv[process.argv.length - 1], 'OMG,OMG');
process.argv.push('OMG,OMG');
process.argv.push('OMG,OMG');
cluster.setupMaster();
