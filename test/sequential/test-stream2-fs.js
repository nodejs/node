'use strict';
var common = require('../common');
var assert = require('assert');

var fs = require('fs');
var FSReadable = fs.ReadStream;

var path = require('path');
var file = path.resolve(common.fixturesDir, 'x1024.txt');

var size = fs.statSync(file).size;

var expectLengths = [1024];

var Stream = require('stream');

class TestWriter extends Stream {
  constructor() {
    super();
    this.buffer = [];
    this.length = 0;
  }

  write(c) {
    this.buffer.push(c.toString());
    this.length += c.length;
    return true;
  }

  end(c) {
    if (c) this.buffer.push(c.toString());
    this.emit('results', this.buffer);
  }
}

var r = new FSReadable(file);
var w = new TestWriter();

w.on('results', function(res) {
  console.error(res, w.length);
  assert.equal(w.length, size);
  assert.deepStrictEqual(res.map(function(c) {
    return c.length;
  }), expectLengths);
  console.log('ok');
});

r.pipe(w);
