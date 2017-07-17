'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const fixtures = require('../common/fixtures');

const exitScript = fixtures.path('exit.js');
const exitChild = spawn(process.argv[0], [exitScript, 23]);
exitChild.on('exit', common.mustCall(function(code, signal) {
  assert.strictEqual(code, 23);
  assert.strictEqual(signal, null);
}));


const errorScript = fixtures.path('child_process_should_emit_error.js');
const errorChild = spawn(process.argv[0], [errorScript]);
errorChild.on('exit', common.mustCall(function(code, signal) {
  assert.ok(code !== 0);
  assert.strictEqual(signal, null);
}));
