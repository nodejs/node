'use strict';
require('../common');
const assert = require('assert');

switch (process.argv[2]) {
  case 'child':
    return child();
  case undefined:
    return parent();
  default:
    throw new Error('invalid');
}

function parent() {
  const spawn = require('child_process').spawn;
  const child = spawn(process.execPath, [__filename, 'child']);

  child.stderr.setEncoding('utf8');
  child.stderr.on('data', function(c) {
    console.error(`${c}`);
    throw new Error('should not get stderr data');
  });

  child.stdout.setEncoding('utf8');
  let out = '';
  child.stdout.on('data', function(c) {
    out += c;
  });
  child.stdout.on('end', function() {
    assert.strictEqual(out, '10\n');
    console.log('ok - got expected output');
  });

  child.on('exit', function(c) {
    assert(!c);
    console.log('ok - exit success');
  });
}

function child() {
  const vm = require('vm');
  let caught;
  try {
    vm.runInThisContext('haf!@##&$!@$*!@', { displayErrors: false });
  } catch (er) {
    caught = true;
  }
  assert(caught);
  vm.runInThisContext('console.log(10)', { displayErrors: false });
}
