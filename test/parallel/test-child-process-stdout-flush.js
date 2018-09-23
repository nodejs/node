'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var spawn = require('child_process').spawn;
var sub = path.join(common.fixturesDir, 'print-chars.js');

var n = 500000;

var child = spawn(process.argv[0], [sub, n]);

var count = 0;

child.stderr.setEncoding('utf8');
child.stderr.on('data', function(data) {
  console.log('parent stderr: ' + data);
  assert.ok(false);
});

child.stdout.setEncoding('utf8');
child.stdout.on('data', function(data) {
  count += data.length;
  console.log(count);
});

child.on('close', function(data) {
  assert.equal(n, count);
  console.log('okay');
});
