'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const path = require('path');

var exitScript = path.join(common.fixturesDir, 'exit.js');
var exitChild = spawn(process.argv[0], [exitScript, 23]);
exitChild.on('exit', common.mustCall(function(code, signal) {
  assert.strictEqual(code, 23);
  assert.strictEqual(signal, null);
}));


var errorScript = path.join(common.fixturesDir,
                            'child_process_should_emit_error.js');
var errorChild = spawn(process.argv[0], [errorScript]);
errorChild.on('exit', common.mustCall(function(code, signal) {
  assert.ok(code !== 0);
  assert.strictEqual(signal, null);
}));
