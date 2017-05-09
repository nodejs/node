'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

const child = spawn(process.argv[0], [`${common.fixturesDir}/should_exit.js`]);
child.stdout.once('data', function() {
  child.kill('SIGINT');
});
child.on('exit', common.mustCall(function(exitCode, signalCode) {
  assert.strictEqual(exitCode, null);
  assert.strictEqual(signalCode, 'SIGINT');
}));
