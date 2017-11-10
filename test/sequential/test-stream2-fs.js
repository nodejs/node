'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');

const fs = require('fs');
const FSReadable = fs.ReadStream;

const path = require('path');
const file = path.resolve(fixtures.path('x1024.txt'));

const size = fs.statSync(file).size;

const expectLengths = [1024];

const Stream = require('stream');

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

const r = new FSReadable(file);
const w = new TestWriter();

w.on('results', function(res) {
  console.error(res, w.length);
  assert.strictEqual(w.length, size);
  assert.deepStrictEqual(res.map(function(c) {
    return c.length;
  }), expectLengths);
  console.log('ok');
});

r.pipe(w);
