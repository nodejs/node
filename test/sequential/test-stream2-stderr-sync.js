'use strict';
// Make sure that sync writes to stderr get processed before exiting.

require('../common');

function parent() {
  var spawn = require('child_process').spawn;
  var assert = require('assert');
  var i = 0;
  children.forEach(function(_, c) {
    var child = spawn(process.execPath, [__filename, '' + c]);
    var err = '';

    child.stderr.on('data', function(c) {
      err += c;
    });

    child.on('close', function() {
      assert.equal(err, 'child ' + c + '\nfoo\nbar\nbaz\n');
      console.log('ok %d child #%d', ++i, c);
      if (i === children.length)
        console.log('1..' + i);
    });
  });
}

// using console.error
function child0() {
  console.error('child 0');
  console.error('foo');
  console.error('bar');
  console.error('baz');
}

// using process.stderr
function child1() {
  process.stderr.write('child 1\n');
  process.stderr.write('foo\n');
  process.stderr.write('bar\n');
  process.stderr.write('baz\n');
}

// using a net socket
function child2() {
  var net = require('net');
  var socket = new net.Socket({
    fd: 2,
    readable: false,
    writable: true});
  socket.write('child 2\n');
  socket.write('foo\n');
  socket.write('bar\n');
  socket.write('baz\n');
}


function child3() {
  console.error('child 3\nfoo\nbar\nbaz');
}

function child4() {
  process.stderr.write('child 4\nfoo\nbar\nbaz\n');
}

var children = [ child0, child1, child2, child3, child4 ];

if (!process.argv[2]) {
  parent();
} else {
  children[process.argv[2]]();
  // immediate process.exit to kill any waiting stuff.
  process.exit();
}
