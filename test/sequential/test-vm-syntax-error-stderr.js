'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var child_process = require('child_process');

var wrong_script = path.join(common.fixturesDir, 'cert.pem');

var p = child_process.spawn(process.execPath, [
  '-e',
  'require(process.argv[1]);',
  wrong_script
]);

p.stdout.on('data', function(data) {
  assert(false, 'Unexpected stdout data: ' + data);
});

var output = '';

p.stderr.on('data', function(data) {
  output += data;
});

process.on('exit', function() {
  assert(/BEGIN CERT/.test(output));
  assert(/^\s+\^/m.test(output));
  assert(/Invalid left-hand side expression in prefix operation/.test(output));
});
