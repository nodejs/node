'use strict';
var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');

setTimeout(common.fail.bind(assert, 'setup not emitted'), 1000).unref();

cluster.on('setup', function() {
  var clusterArgs = cluster.settings.args;
  var realArgs = process.argv;
  assert.equal(clusterArgs[clusterArgs.length - 1],
               realArgs[realArgs.length - 1]);
});

assert(process.argv[process.argv.length - 1] !== 'OMG,OMG');
process.argv.push('OMG,OMG');
process.argv.push('OMG,OMG');
cluster.setupMaster();
