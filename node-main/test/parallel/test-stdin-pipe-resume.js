'use strict';
// This tests that piping stdin will cause it to resume() as well.
require('../common');
const assert = require('assert');

if (process.argv[2] === 'child') {
  process.stdin.pipe(process.stdout);
} else {
  const spawn = require('child_process').spawn;
  const buffers = [];
  const child = spawn(process.execPath, [__filename, 'child']);
  child.stdout.on('data', function(c) {
    buffers.push(c);
  });
  child.stdout.on('close', function() {
    const b = Buffer.concat(buffers).toString();
    assert.strictEqual(b, 'Hello, world\n');
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
