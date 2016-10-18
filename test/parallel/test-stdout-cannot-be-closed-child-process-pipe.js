'use strict';
require('../common');
var assert = require('assert');

if (process.argv[2] === 'child')
  process.stdout.end('foo');
else
  parent();

function parent() {
  var spawn = require('child_process').spawn;
  var child = spawn(process.execPath, [__filename, 'child']);
  var out = '';
  var err = '';

  child.stdout.setEncoding('utf8');
  child.stderr.setEncoding('utf8');

  child.stdout.on('data', function(c) {
    out += c;
  });
  child.stderr.on('data', function(c) {
    err += c;
  });

  child.on('close', function(code, signal) {
    assert.strictEqual(code, 0, 'exit code should be zero');
    assert.equal(out, 'foo');
    assert.strictEqual(err, '', 'stderr should be empty');
    console.log('ok');
  });
}
