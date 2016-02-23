'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var child = spawn(process.argv[0], [common.fixturesDir + '/should_exit.js']);
child.stdout.once('data', function() {
  child.kill('SIGINT');
});
child.on('exit', common.mustCall(function(exitCode, signalCode) {
  assert.equal(exitCode, null);
  assert.equal(signalCode, 'SIGINT');
}));
