'use strict';
// This tests that piping stdin will cause it to resume() as well.
require('../common');
var assert = require('assert');

if (process.argv[2] === 'child') {
  process.stdin.pipe(process.stdout);
} else {
  var spawn = require('child_process').spawn;
  var buffers = [];
  var child = spawn(process.execPath, [__filename, 'child']);
  child.stdout.on('data', function(c) {
    buffers.push(c);
  });
  child.stdout.on('close', function() {
    var b = Buffer.concat(buffers).toString();
    assert.equal(b, 'Hello, world\n');
    console.log('ok');
  });
  child.stdin.write('Hel');
  child.stdin.write('lo,');
  child.stdin.write(' wo');
  setTimeout(function() {
    child.stdin.write('rld\n');
    child.stdin.end();
  }, 10);
}
