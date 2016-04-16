'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var path = require('path');

var exits = 0;

var exitScript = path.join(common.fixturesDir, 'exit.js');
var exitChild = spawn(process.argv[0], [exitScript, 23]);
exitChild.on('exit', function(code, signal) {
  assert.strictEqual(code, 23);
  assert.strictEqual(signal, null);

  exits++;
});


var errorScript = path.join(common.fixturesDir,
                            'child_process_should_emit_error.js');
var errorChild = spawn(process.argv[0], [errorScript]);
errorChild.on('exit', function(code, signal) {
  assert.ok(code !== 0);
  assert.strictEqual(signal, null);

  exits++;
});


process.on('exit', function() {
  assert.equal(2, exits);
});
