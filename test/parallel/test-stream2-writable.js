// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
require('../common');
const W = require('_stream_writable');
const D = require('_stream_duplex');
const assert = require('assert');

const util = require('util');
util.inherits(TestWriter, W);

function TestWriter() {
  W.apply(this, arguments);
  this.buffer = [];
  this.written = 0;
}

TestWriter.prototype._write = function(chunk, encoding, cb) {
  // simulate a small unpredictable latency
  setTimeout(function() {
    this.buffer.push(chunk.toString());
    this.written += chunk.length;
    cb();
  }.bind(this), Math.floor(Math.random() * 10));
};

const chunks = new Array(50);
for (let i = 0; i < chunks.length; i++) {
  chunks[i] = 'x'.repeat(i);
}

// tiny node-tap lookalike.
const tests = [];
let count = 0;

function test(name, fn) {
  count++;
  tests.push([name, fn]);
}

function run() {
  const next = tests.shift();
  if (!next)
    return console.error('ok');

  const name = next[0];
  const fn = next[1];
  console.log('# %s', name);
  fn({
    same: assert.deepStrictEqual,
    equal: assert.strictEqual,
    end: function() {
      count--;
      run();
    }
  });
}

// ensure all tests have run
process.on('exit', function() {
  assert.strictEqual(count, 0);
});

process.nextTick(run);

test('write fast', function(t) {
  const tw = new TestWriter({
    highWaterMark: 100
  });

  tw.on('finish', function() {
    t.same(tw.buffer, chunks, 'got chunks in the right order');
    t.end();
  });

  chunks.forEach(function(chunk) {
    // screw backpressure.  Just buffer it all up.
    tw.write(chunk);
  });
  tw.end();
});

test('write slow', function(t) {
  const tw = new TestWriter({
    highWaterMark: 100
  });

  tw.on('finish', function() {
    t.same(tw.buffer, chunks, 'got chunks in the right order');
    t.end();
  });

  let i = 0;
  (function W() {
    tw.write(chunks[i++]);
    if (i < chunks.length)
      setTimeout(W, 10);
    else
      tw.end();
  })();
});

test('write backpressure', function(t) {
  const tw = new TestWriter({
    highWaterMark: 50
  });

  let drains = 0;

  tw.on('finish', function() {
    t.same(tw.buffer, chunks, 'got chunks in the right order');
    t.equal(drains, 17);
    t.end();
  });

  tw.on('drain', function() {
    drains++;
  });

  let i = 0;
  (function W() {
    let ret;
    do {
      ret = tw.write(chunks[i++]);
    } while (ret !== false && i < chunks.length);

    if (i < chunks.length) {
      assert(tw._writableState.length >= 50);
      tw.once('drain', W);
    } else {
      tw.end();
    }
  })();
});

test('write bufferize', function(t) {
  const tw = new TestWriter({
    highWaterMark: 100
  });

  const encodings =
    [ 'hex',
      'utf8',
      'utf-8',
      'ascii',
      'latin1',
      'binary',
      'base64',
      'ucs2',
      'ucs-2',
      'utf16le',
      'utf-16le',
      undefined ];

  tw.on('finish', function() {
    t.same(tw.buffer, chunks, 'got the expected chunks');
  });

  chunks.forEach(function(chunk, i) {
    const enc = encodings[i % encodings.length];
    chunk = Buffer.from(chunk);
    tw.write(chunk.toString(enc), enc);
  });
  t.end();
});

test('write no bufferize', function(t) {
  const tw = new TestWriter({
    highWaterMark: 100,
    decodeStrings: false
  });

  tw._write = function(chunk, encoding, cb) {
    assert.strictEqual(typeof chunk, 'string');
    chunk = Buffer.from(chunk, encoding);
    return TestWriter.prototype._write.call(this, chunk, encoding, cb);
  };

  const encodings =
    [ 'hex',
      'utf8',
      'utf-8',
      'ascii',
      'latin1',
      'binary',
      'base64',
      'ucs2',
      'ucs-2',
      'utf16le',
      'utf-16le',
      undefined ];

  tw.on('finish', function() {
    t.same(tw.buffer, chunks, 'got the expected chunks');
  });

  chunks.forEach(function(chunk, i) {
    const enc = encodings[i % encodings.length];
    chunk = Buffer.from(chunk);
    tw.write(chunk.toString(enc), enc);
  });
  t.end();
});

test('write callbacks', function(t) {
  const callbacks = chunks.map(function(chunk, i) {
    return [i, function() {
      callbacks._called[i] = chunk;
    }];
  }).reduce(function(set, x) {
    set['callback-' + x[0]] = x[1];
    return set;
  }, {});
  callbacks._called = [];

  const tw = new TestWriter({
    highWaterMark: 100
  });

  tw.on('finish', function() {
    process.nextTick(function() {
      t.same(tw.buffer, chunks, 'got chunks in the right order');
      t.same(callbacks._called, chunks, 'called all callbacks');
      t.end();
    });
  });

  chunks.forEach(function(chunk, i) {
    tw.write(chunk, callbacks['callback-' + i]);
  });
  tw.end();
});

test('end callback', function(t) {
  const tw = new TestWriter();
  tw.end(function() {
    t.end();
  });
});

test('end callback with chunk', function(t) {
  const tw = new TestWriter();
  tw.end(Buffer.from('hello world'), function() {
    t.end();
  });
});

test('end callback with chunk and encoding', function(t) {
  const tw = new TestWriter();
  tw.end('hello world', 'ascii', function() {
    t.end();
  });
});

test('end callback after .write() call', function(t) {
  const tw = new TestWriter();
  tw.write(Buffer.from('hello world'));
  tw.end(function() {
    t.end();
  });
});

test('end callback called after write callback', function(t) {
  const tw = new TestWriter();
  let writeCalledback = false;
  tw.write(Buffer.from('hello world'), function() {
    writeCalledback = true;
  });
  tw.end(function() {
    t.equal(writeCalledback, true);
    t.end();
  });
});

test('encoding should be ignored for buffers', function(t) {
  const tw = new W();
  const hex = '018b5e9a8f6236ffe30e31baf80d2cf6eb';
  tw._write = function(chunk) {
    t.equal(chunk.toString('hex'), hex);
    t.end();
  };
  const buf = Buffer.from(hex, 'hex');
  tw.write(buf, 'latin1');
});

test('writables are not pipable', function(t) {
  const w = new W();
  w._write = function() {};
  let gotError = false;
  w.on('error', function() {
    gotError = true;
  });
  w.pipe(process.stdout);
  assert(gotError);
  t.end();
});

test('duplexes are pipable', function(t) {
  const d = new D();
  d._read = function() {};
  d._write = function() {};
  let gotError = false;
  d.on('error', function() {
    gotError = true;
  });
  d.pipe(process.stdout);
  assert(!gotError);
  t.end();
});

test('end(chunk) two times is an error', function(t) {
  const w = new W();
  w._write = function() {};
  let gotError = false;
  w.on('error', function(er) {
    gotError = true;
    t.equal(er.message, 'write after end');
  });
  w.end('this is the end');
  w.end('and so is this');
  process.nextTick(function() {
    assert(gotError);
    t.end();
  });
});

test('dont end while writing', function(t) {
  const w = new W();
  let wrote = false;
  w._write = function(chunk, e, cb) {
    assert(!this.writing);
    wrote = true;
    this.writing = true;
    setTimeout(function() {
      this.writing = false;
      cb();
    }, 1);
  };
  w.on('finish', function() {
    assert(wrote);
    t.end();
  });
  w.write(Buffer.alloc(0));
  w.end();
});

test('finish does not come before write cb', function(t) {
  const w = new W();
  let writeCb = false;
  w._write = function(chunk, e, cb) {
    setTimeout(function() {
      writeCb = true;
      cb();
    }, 10);
  };
  w.on('finish', function() {
    assert(writeCb);
    t.end();
  });
  w.write(Buffer.alloc(0));
  w.end();
});

test('finish does not come before sync _write cb', function(t) {
  const w = new W();
  let writeCb = false;
  w._write = function(chunk, e, cb) {
    cb();
  };
  w.on('finish', function() {
    assert(writeCb);
    t.end();
  });
  w.write(Buffer.alloc(0), function() {
    writeCb = true;
  });
  w.end();
});

test('finish is emitted if last chunk is empty', function(t) {
  const w = new W();
  w._write = function(chunk, e, cb) {
    process.nextTick(cb);
  };
  w.on('finish', function() {
    t.end();
  });
  w.write(Buffer.allocUnsafe(1));
  w.end(Buffer.alloc(0));
});
