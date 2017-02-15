'use strict';
require('../common');
const assert = require('assert');

if (process.argv[2] === 'child')
  process.stdout.end('foo');
else
  parent();

function parent() {
  const spawn = require('child_process').spawn;
  const child = spawn(process.execPath, [__filename, 'child']);
  let out = '';
  let err = '';

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
    assert.strictEqual(out, 'foo');
    assert(/process\.stdout cannot be closed/.test(err));
    console.log('ok');
  });
}
