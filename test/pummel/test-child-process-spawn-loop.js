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
  child.stdout.addListener('data', function(chunk) {
    count += chunk.length;
  });

  child.stderr.addListener('data', function(chunk) {
    console.log('stderr: ' + chunk);
  });

  child.addListener('exit', function() {
    assert.equal(SIZE + 1, count); // + 1 for \n
    if (i < N) {
      doSpawn(i + 1);
    } else {
      finished = true;
    }
  });
}

doSpawn(0);

process.addListener('exit', function() {
  assert.ok(finished);
});
