'use strict';
var common = require('../common');
var assert = require('assert');

var Stream = require('stream');
var Readable = Stream.Readable;

var r = new Readable();
var N = 256;
var reads = 0;
r._read = function(n) {
  return r.push(++reads === N ? null : new Buffer(1));
};

var rended = false;
r.on('end', function() {
  rended = true;
});

var w = new Stream();
w.writable = true;
var writes = 0;
var buffered = 0;
w.write = function(c) {
  writes += c.length;
  buffered += c.length;
  process.nextTick(drain);
  return false;
};

function drain() {
  assert(buffered <= 3);
  buffered = 0;
  w.emit('drain');
}


var wended = false;
w.end = function() {
  wended = true;
};

// Just for kicks, let's mess with the drain count.
// This verifies that even if it gets negative in the
// pipe() cleanup function, we'll still function properly.
r.on('readable', function() {
  w.emit('drain');
});

r.pipe(w);
process.on('exit', function() {
  assert(rended);
  assert(wended);
  console.error('ok');
});
