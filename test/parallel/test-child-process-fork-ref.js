'use strict';
require('../common');
var assert = require('assert');
var fork = require('child_process').fork;

if (process.argv[2] === 'child') {
  process.send('1');

  // check that child don't instantly die
  setTimeout(function() {
    process.send('2');
  }, 200);

  process.on('disconnect', function() {
    process.stdout.write('3');
  });

} else {
  var child = fork(__filename, ['child'], {silent: true});

  var ipc = [], stdout = '';

  child.on('message', function(msg) {
    ipc.push(msg);

    if (msg === '2') child.disconnect();
  });

  child.stdout.on('data', function(chunk) {
    stdout += chunk;
  });

  child.once('exit', function() {
    assert.deepStrictEqual(ipc, ['1', '2']);
    assert.equal(stdout, '3');
  });
}
