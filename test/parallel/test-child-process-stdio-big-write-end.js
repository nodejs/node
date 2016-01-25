'use strict';
require('../common');
var assert = require('assert');
var BUFSIZE = 1024;

switch (process.argv[2]) {
  case undefined:
    return parent();
  case 'child':
    return child();
  default:
    throw new Error('wtf?');
}

function parent() {
  var spawn = require('child_process').spawn;
  var child = spawn(process.execPath, [__filename, 'child']);
  var sent = 0;

  var n = '';
  child.stdout.setEncoding('ascii');
  child.stdout.on('data', function(c) {
    n += c;
  });
  child.stdout.on('end', function() {
    assert.equal(+n, sent);
    console.log('ok');
  });

  // Write until the buffer fills up.
  do {
    var buf = Buffer.alloc(BUFSIZE, '.');
    sent += BUFSIZE;
  } while (child.stdin.write(buf));

  // then write a bunch more times.
  for (var i = 0; i < 100; i++) {
    const buf = Buffer.alloc(BUFSIZE, '.');
    sent += BUFSIZE;
    child.stdin.write(buf);
  }

  // now end, before it's all flushed.
  child.stdin.end();

  // now we wait...
}

function child() {
  var received = 0;
  process.stdin.on('data', function(c) {
    received += c.length;
  });
  process.stdin.on('end', function() {
    console.log(received);
  });
}
