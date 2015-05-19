'use strict';
var common = require('../common');
var R = require('_stream_readable');
var assert = require('assert');

var fs = require('fs');
var FSReadable = fs.ReadStream;

var path = require('path');
var file = path.resolve(common.fixturesDir, 'x1024.txt');

var size = fs.statSync(file).size;

var expectLengths = [1024];

var util = require('util');
var Stream = require('stream');

util.inherits(TestWriter, Stream);

function TestWriter() {
  Stream.apply(this);
  this.buffer = [];
  this.length = 0;
}

TestWriter.prototype.write = function(c) {
  this.buffer.push(c.toString());
  this.length += c.length;
  return true;
};

TestWriter.prototype.end = function(c) {
  if (c) this.buffer.push(c.toString());
  this.emit('results', this.buffer);
};

var r = new FSReadable(file);
var w = new TestWriter();

w.on('results', function(res) {
  console.error(res, w.length);
  assert.equal(w.length, size);
  var l = 0;
  assert.deepEqual(res.map(function(c) {
    return c.length;
  }), expectLengths);
  console.log('ok');
});

r.pipe(w);
