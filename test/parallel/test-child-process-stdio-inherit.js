'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

if (process.argv[2] === 'parent')
  parent();
else
  grandparent();

function grandparent() {
  var child = spawn(process.execPath, [__filename, 'parent']);
  child.stderr.pipe(process.stderr);
  var output = '';
  var input = 'asdfasdf';

  child.stdout.on('data', function(chunk) {
    output += chunk;
  });
  child.stdout.setEncoding('utf8');

  child.stdin.end(input);

  child.on('close', function(code, signal) {
    assert.equal(code, 0);
    assert.equal(signal, null);
    // cat on windows adds a \r\n at the end.
    assert.equal(output.trim(), input.trim());
  });
}

function parent() {
  // should not immediately exit.
  common.spawnCat({ stdio: 'inherit' });
}
