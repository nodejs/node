'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var child_process = require('child_process');

var wrong_script = path.join(common.fixturesDir, 'cert.pem');

var p = child_process.spawn(process.execPath, [
  '-e',
  'try { require(process.argv[1]); } catch (e) { console.log(e.stack); }',
  wrong_script
]);

p.stderr.on('data', function(data) {
  assert(false, 'Unexpected stderr data: ' + data);
});

var output = '';

p.stdout.on('data', function(data) {
  output += data;
});

process.on('exit', function() {
  assert(/BEGIN CERT/.test(output));
  assert(/^\s+\^/m.test(output));
  assert(/Invalid left-hand side expression in prefix operation/.test(output));
});
