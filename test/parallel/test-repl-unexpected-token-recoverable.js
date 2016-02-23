'use strict';
/*
 * This is a regression test for https://github.com/joyent/node/issues/8874.
 */
require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;
// use -i to force node into interactive mode, despite stdout not being a TTY
var args = [ '-i' ];
var child = spawn(process.execPath, args);

var input = 'var foo = "bar\\\nbaz"';
// Match '...' as well since it marks a multi-line statement
var expectOut = /^> ... undefined\n/;

child.stderr.setEncoding('utf8');
child.stderr.on('data', function(c) {
  throw new Error('child.stderr be silent');
});

child.stdout.setEncoding('utf8');
var out = '';
child.stdout.on('data', function(c) {
  out += c;
});

child.stdout.on('end', function() {
  assert(expectOut.test(out));
  console.log('ok');
});

child.stdin.end(input);
