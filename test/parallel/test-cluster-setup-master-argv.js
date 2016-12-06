'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

setTimeout(common.fail.bind(assert, 'setup not emitted'), 1000).unref();

cluster.on('setup', common.mustCall(function() {
  var clusterArgs = cluster.settings.args;
  var realArgs = process.argv;
  assert.strictEqual(clusterArgs[clusterArgs.length - 1],
                     realArgs[realArgs.length - 1]);
}));

assert.notStrictEqual(process.argv[process.argv.length - 1], 'OMG,OMG');
process.argv.push('OMG,OMG');
process.argv.push('OMG,OMG');
cluster.setupMaster();
