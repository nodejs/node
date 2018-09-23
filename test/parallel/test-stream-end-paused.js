'use strict';
var common = require('../common');
var assert = require('assert');
var gotEnd = false;

// Make sure we don't miss the end event for paused 0-length streams

var Readable = require('stream').Readable;
var stream = new Readable();
var calledRead = false;
stream._read = function() {
  assert(!calledRead);
  calledRead = true;
  this.push(null);
};

stream.on('data', function() {
  throw new Error('should not ever get data');
});
stream.pause();

setTimeout(function() {
  stream.on('end', function() {
    gotEnd = true;
  });
  stream.resume();
});

process.on('exit', function() {
  assert(gotEnd);
  assert(calledRead);
  console.log('ok');
});
