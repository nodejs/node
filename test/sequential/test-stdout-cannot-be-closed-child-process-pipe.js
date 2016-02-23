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
    assert(code);
    assert.equal(out, 'foo');
    assert(/process\.stdout cannot be closed/.test(err));
    console.log('ok');
  });
}
