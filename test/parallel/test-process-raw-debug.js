'use strict';
require('../common');
const assert = require('assert');
const os = require('os');

switch (process.argv[2]) {
  case 'child':
    return child();
  case undefined:
    return parent();
  default:
    throw new Error('wtf? ' + process.argv[2]);
}

function parent() {
  const spawn = require('child_process').spawn;
  const child = spawn(process.execPath, [__filename, 'child']);

  let output = '';

  child.stderr.on('data', function(c) {
    output += c;
  });

  child.stderr.setEncoding('utf8');

  child.stderr.on('end', function() {
    assert.equal(output, 'I can still debug!' + os.EOL);
    console.log('ok - got expected message');
  });

  child.on('exit', function(c) {
    assert(!c);
    console.log('ok - child exited nicely');
  });
}

function child() {
  // even when all hope is lost...

  process.nextTick = function() {
    throw new Error('No ticking!');
  };

  const stderr = process.stderr;
  stderr.write = function() {
    throw new Error('No writing to stderr!');
  };

  process._rawDebug('I can still %s!', 'debug');
}
