'use strict';
var common = require('../common');
var assert = require('assert');

var exec = require('child_process').exec;
var join = require('path').join;

var nodePath = process.argv[0];
var script = join(common.fixturesDir, 'print-10-lines.js');

var cmd = '"' + nodePath + '" "' + script + '" | head -2';

exec(cmd, common.mustCall(function(err, stdout, stderr) {
  if (err) throw err;
  var lines = stdout.split('\n');
  assert.equal(3, lines.length);
}));
