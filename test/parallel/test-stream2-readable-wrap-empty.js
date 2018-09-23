'use strict';
var common = require('../common');
var assert = require('assert');

var Readable = require('_stream_readable');
var EE = require('events').EventEmitter;

var oldStream = new EE();
oldStream.pause = function() {};
oldStream.resume = function() {};

var newStream = new Readable().wrap(oldStream);

var ended = false;
newStream
  .on('readable', function() {})
  .on('end', function() { ended = true; });

oldStream.emit('end');

process.on('exit', function() {
  assert.ok(ended);
});
