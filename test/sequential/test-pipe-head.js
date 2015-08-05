'use strict';
const common = require('../common');
const assert = require('assert');

const exec = require('child_process').exec;
const join = require('path').join;

var nodePath = process.argv[0];
var script = join(common.fixturesDir, 'print-10-lines.js');

var cmd = '"' + nodePath + '" "' + script + '" | head -2';

var finished = false;

exec(cmd, function(err, stdout, stderr) {
  if (err) throw err;
  var lines = stdout.split('\n');
  assert.equal(3, lines.length);
  finished = true;
});


process.on('exit', function() {
  assert.ok(finished);
});
