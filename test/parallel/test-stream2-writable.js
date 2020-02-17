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

const common = require('../common');
const W = require('_stream_writable');
const D = require('_stream_duplex');
const assert = require('assert');

class TestWriter extends W {
  constructor(opts) {
    super(opts);
    this.buffer = [];
    this.written = 0;
  }

  _write(chunk, encoding, cb) {
    // Simulate a small unpredictable latency
    setTimeout(() => {
      this.buffer.push(chunk.toString());
      this.written += chunk.length;
      cb();
    }, Math.floor(Math.random() * 10));
  }
}

const chunks = new Array(50);
for (let i = 0; i < chunks.length; i++) {
  chunks[i] = 'x'.repeat(i);
}

{
  // Verify fast writing
  const tw = new TestWriter({
    highWaterMark: 100
  });

  tw.on('finish', common.mustCall(function() {
    // Got chunks in the right order
    assert.deepStrictEqual(tw.buffer, chunks);
  }));

  chunks.forEach(function(chunk) {
    // Ignore backpressure. Just buffer it all up.
    tw.write(chunk);
  });
  tw.end();
}

{
  // Verify slow writing
  const tw = new TestWriter({
    highWaterMark: 100
  });

  tw.on('finish', common.mustCall(function() {
    //  Got chunks in the right order
    assert.deepStrictEqual(tw.buffer, chunks);
  }));

  let i = 0;
  (function W() {
    tw.write(chunks[i++]);
    if (i < chunks.length)
      setTimeout(W, 10);
    else
      tw.end();
  })();
}

{
  // Verify write backpressure
  const tw = new TestWriter({
    highWaterMark: 50
  });

  let drains = 0;

  tw.on('finish', common.mustCall(function() {
    // Got chunks in the right order
    assert.deepStrictEqual(tw.buffer, chunks);
    assert.strictEqual(drains, 17);
  }));

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
      assert(tw.writableLength >= 50);
      tw.once('drain', W);
    } else {
      tw.end();
    }
  })();
}

{
  // Verify write buffersize
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
    // Got the expected chunks
    assert.deepStrictEqual(tw.buffer, chunks);
  });

  chunks.forEach(function(chunk, i) {
    const enc = encodings[i % encodings.length];
    chunk = Buffer.from(chunk);
    tw.write(chunk.toString(enc), enc);
  });
}

{
  // Verify write with no buffersize
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
    // Got the expected chunks
    assert.deepStrictEqual(tw.buffer, chunks);
  });

  chunks.forEach(function(chunk, i) {
    const enc = encodings[i % encodings.length];
    chunk = Buffer.from(chunk);
    tw.write(chunk.toString(enc), enc);
  });
}

{
  // Verify write callbacks
  const callbacks = chunks.map(function(chunk, i) {
    return [i, function() {
      callbacks._called[i] = chunk;
    }];
  }).reduce(function(set, x) {
    set[`callback-${x[0]}`] = x[1];
    return set;
  }, {});
  callbacks._called = [];

  const tw = new TestWriter({
    highWaterMark: 100
  });

  tw.on('finish', common.mustCall(function() {
    process.nextTick(common.mustCall(function() {
      // Got chunks in the right order
      assert.deepStrictEqual(tw.buffer, chunks);
      // Called all callbacks
      assert.deepStrictEqual(callbacks._called, chunks);
    }));
  }));

  chunks.forEach(function(chunk, i) {
    tw.write(chunk, callbacks[`callback-${i}`]);
  });
  tw.end();
}

{
  // Verify end() callback
  const tw = new TestWriter();
  tw.end(common.mustCall());
}

const helloWorldBuffer = Buffer.from('hello world');

{
  // Verify end() callback with chunk
  const tw = new TestWriter();
  tw.end(helloWorldBuffer, common.mustCall());
}

{
  // Verify end() callback with chunk and encoding
  const tw = new TestWriter();
  tw.end('hello world', 'ascii', common.mustCall());
}

{
  // Verify end() callback after write() call
  const tw = new TestWriter();
  tw.write(helloWorldBuffer);
  tw.end(common.mustCall());
}

{
  // Verify end() callback after write() callback
  const tw = new TestWriter();
  let writeCalledback = false;
  tw.write(helloWorldBuffer, function() {
    writeCalledback = true;
  });
  tw.end(common.mustCall(function() {
    assert.strictEqual(writeCalledback, true);
  }));
}

{
  // Verify encoding is ignored for buffers
  const tw = new W();
  const hex = '018b5e9a8f6236ffe30e31baf80d2cf6eb';
  tw._write = common.mustCall(function(chunk) {
    assert.strictEqual(chunk.toString('hex'), hex);
  });
  const buf = Buffer.from(hex, 'hex');
  tw.write(buf, 'latin1');
}

{
  // Verify writables cannot be piped
  const w = new W({ autoDestroy: false });
  w._write = common.mustNotCall();
  let gotError = false;
  w.on('error', function() {
    gotError = true;
  });
  w.pipe(process.stdout);
  assert.strictEqual(gotError, true);
}

{
  // Verify that duplex streams cannot be piped
  const d = new D();
  d._read = common.mustCall();
  d._write = common.mustNotCall();
  let gotError = false;
  d.on('error', function() {
    gotError = true;
  });
  d.pipe(process.stdout);
  assert.strictEqual(gotError, false);
}

{
  // Verify that end(chunk) twice is an error
  const w = new W();
  w._write = common.mustCall((msg) => {
    assert.strictEqual(msg.toString(), 'this is the end');
  });
  let gotError = false;
  w.on('error', function(er) {
    gotError = true;
    assert.strictEqual(er.message, 'write after end');
  });
  w.end('this is the end');
  w.end('and so is this');
  process.nextTick(common.mustCall(function() {
    assert.strictEqual(gotError, true);
  }));
}

{
  // Verify stream doesn't end while writing
  const w = new W();
  let wrote = false;
  w._write = function(chunk, e, cb) {
    assert.strictEqual(this.writing, undefined);
    wrote = true;
    this.writing = true;
    setTimeout(() => {
      this.writing = false;
      cb();
    }, 1);
  };
  w.on('finish', common.mustCall(function() {
    assert.strictEqual(wrote, true);
    assert.strictEqual(this.writing, false);
  }));
  w.write(Buffer.alloc(0));
  w.end();
}

{
  // Verify finish does not come before write() callback
  const w = new W();
  let writeCb = false;
  w._write = function(chunk, e, cb) {
    setTimeout(function() {
      writeCb = true;
      cb();
    }, 10);
  };
  w.on('finish', common.mustCall(function() {
    assert.strictEqual(writeCb, true);
  }));
  w.write(Buffer.alloc(0));
  w.end();
}

{
  // Verify finish does not come before synchronous _write() callback
  const w = new W();
  let writeCb = false;
  w._write = function(chunk, e, cb) {
    cb();
  };
  w.on('finish', common.mustCall(function() {
    assert.strictEqual(writeCb, true);
  }));
  w.write(Buffer.alloc(0), function() {
    writeCb = true;
  });
  w.end();
}

{
  // Verify finish is emitted if the last chunk is empty
  const w = new W();
  w._write = function(chunk, e, cb) {
    process.nextTick(cb);
  };
  w.on('finish', common.mustCall());
  w.write(Buffer.allocUnsafe(1));
  w.end(Buffer.alloc(0));
}

{
  // Verify that finish is emitted after shutdown
  const w = new W();
  let shutdown = false;

  w._final = common.mustCall(function(cb) {
    assert.strictEqual(this, w);
    setTimeout(function() {
      shutdown = true;
      cb();
    }, 100);
  });
  w._write = function(chunk, e, cb) {
    process.nextTick(cb);
  };
  w.on('finish', common.mustCall(function() {
    assert.strictEqual(shutdown, true);
  }));
  w.write(Buffer.allocUnsafe(1));
  w.end(Buffer.allocUnsafe(0));
}

{
  // Verify that error is only emitted once when failing in _finish.
  const w = new W();

  w._final = common.mustCall(function(cb) {
    cb(new Error('test'));
  });
  w.on('error', common.mustCall((err) => {
    assert.strictEqual(w._writableState.errorEmitted, true);
    assert.strictEqual(err.message, 'test');
    w.on('error', common.mustNotCall());
    w.destroy(new Error());
  }));
  w.end();
}

{
  // Verify that error is only emitted once when failing in write.
  const w = new W();
  w.on('error', common.mustNotCall());
  assert.throws(() => {
    w.write(null);
  }, {
    code: 'ERR_STREAM_NULL_VALUES'
  });
}

{
  // Verify that error is only emitted once when failing in write after end.
  const w = new W();
  w.on('error', common.mustCall((err) => {
    assert.strictEqual(w._writableState.errorEmitted, true);
    assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
  }));
  w.end();
  w.write('hello');
  w.destroy(new Error());
}

{
  // Verify that finish is not emitted after error
  const w = new W();

  w._final = common.mustCall(function(cb) {
    cb(new Error());
  });
  w._write = function(chunk, e, cb) {
    process.nextTick(cb);
  };
  w.on('error', common.mustCall());
  w.on('prefinish', common.mustNotCall());
  w.on('finish', common.mustNotCall());
  w.write(Buffer.allocUnsafe(1));
  w.end(Buffer.allocUnsafe(0));
}
