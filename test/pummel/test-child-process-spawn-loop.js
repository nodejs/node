'use strict';
var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

var SIZE = 1000 * 1024;
var N = 40;
var finished = false;

function doSpawn(i) {
  var child = spawn('python', ['-c', 'print ' + SIZE + ' * "C"']);
  var count = 0;

  child.stdout.setEncoding('ascii');
  child.stdout.on('data', function(chunk) {
    count += chunk.length;
  });

  child.stderr.on('data', function(chunk) {
    console.log('stderr: ' + chunk);
  });

  child.on('close', function() {
    // + 1 for \n or + 2 for \r\n on Windows
    assert.equal(SIZE + (common.isWindows ? 2 : 1), count);
    if (i < N) {
      doSpawn(i + 1);
    } else {
      finished = true;
    }
  });
}

doSpawn(0);

process.on('exit', function() {
  assert.ok(finished);
});
