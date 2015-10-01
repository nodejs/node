'use strict';
var common = require('../common');
var assert = require('assert');

switch (process.argv[2]) {
  case 'child':
    return child();
  case undefined:
    return parent();
  default:
    throw new Error('wtf');
}

function parent() {
  var spawn = require('child_process').spawn;
  var child = spawn(process.execPath, [__filename, 'child']);

  child.stderr.setEncoding('utf8');
  child.stderr.on('data', function(c) {
    console.error('%j', c);
    throw new Error('should not get stderr data');
  });

  child.stdout.setEncoding('utf8');
  var out = '';
  child.stdout.on('data', function(c) {
    out += c;
  });
  child.stdout.on('end', function() {
    assert.equal(out, '10\n');
    console.log('ok - got expected output');
  });

  child.on('exit', function(c) {
    assert(!c);
    console.log('ok - exit success');
  });
}

function child() {
  var vm = require('vm');
  assert.throws(() =>
      vm.runInThisContext('haf!@##&$!@$*!@', { displayErrors: false }), Error);
  vm.runInThisContext('console.log(10)', { displayErrors: false });
}
