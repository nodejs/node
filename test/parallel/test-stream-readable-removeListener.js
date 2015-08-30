'use strict';
var assert = require('assert');
var EE = require('events').EventEmitter;
var Readable = require('stream').Readable;

var buf = new Buffer(65536);
buf.fill(0);

var packetsToPush = 10;
var toUnshift = 16 * 1024;

Readable.prototype._read = function() {
  var chunk = --packetsToPush > 0 ? buf.slice() : null;
  setImmediate(this.push.bind(this, chunk));
};

var rStream = new Readable();

var first = true;
rStream.on('readable', function onReadable() {
  var data = rStream.read();
  if (first) {
    first = false;
    rStream.removeListener('readable', onReadable);
    assert.equal(EE.listenerCount(rStream, 'readable'), 0);
    rStream.unshift(buf.slice(0, toUnshift));
    rStream.on('readable', onReadable);
  }
});

process.on('exit', function() {
  assert.equal(packetsToPush, 0);
});

rStream.read(0);
