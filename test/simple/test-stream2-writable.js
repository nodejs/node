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

var common = require('../common.js');
var W = require('_stream_writable');
var assert = require('assert');

var util = require('util');
util.inherits(TestWriter, W);

function TestWriter() {
  W.apply(this, arguments);
  this.buffer = [];
  this.written = 0;
}

TestWriter.prototype._write = function(chunk, cb) {
  // simulate a small unpredictable latency
  setTimeout(function() {
    this.buffer.push(chunk.toString());
    this.written += chunk.length;
    cb();
  }.bind(this), Math.floor(Math.random() * 10));
};

var chunks = new Array(50);
for (var i = 0; i < chunks.length; i++) {
  chunks[i] = new Array(i + 1).join('x');
}

// tiny node-tap lookalike.
var tests = [];
function test(name, fn) {
  tests.push([name, fn]);
}

function run() {
  var next = tests.shift();
  if (!next)
    return console.log('ok');

  var name = next[0];
  var fn = next[1];
  console.log('# %s', name);
  fn({
    same: assert.deepEqual,
    equal: assert.equal,
    end: run
  });
}

process.nextTick(run);

test('write fast', function(t) {
  var tw = new TestWriter({
    lowWaterMark: 5,
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
  var tw = new TestWriter({
    lowWaterMark: 5,
    highWaterMark: 100
  });

  tw.on('finish', function() {
    t.same(tw.buffer, chunks, 'got chunks in the right order');
    t.end();
  });

  var i = 0;
  (function W() {
    tw.write(chunks[i++]);
    if (i < chunks.length)
      setTimeout(W, 10);
    else
      tw.end();
  })();
});

test('write backpressure', function(t) {
  var tw = new TestWriter({
    lowWaterMark: 5,
    highWaterMark: 50
  });

  var drains = 0;

  tw.on('finish', function() {
    t.same(tw.buffer, chunks, 'got chunks in the right order');
    t.equal(drains, 17);
    t.end();
  });

  tw.on('drain', function() {
    drains++;
  });

  var i = 0;
  (function W() {
    do {
      var ret = tw.write(chunks[i++]);
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
  var tw = new TestWriter({
    lowWaterMark: 5,
    highWaterMark: 100
  });

  var encodings =
      [ 'hex',
        'utf8',
        'utf-8',
        'ascii',
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
    var enc = encodings[ i % encodings.length ];
    chunk = new Buffer(chunk);
    tw.write(chunk.toString(enc), enc);
  });
  t.end();
});

test('write no bufferize', function(t) {
  var tw = new TestWriter({
    lowWaterMark: 5,
    highWaterMark: 100,
    decodeStrings: false
  });

  tw._write = function(chunk, cb) {
    assert(Array.isArray(chunk));
    assert(typeof chunk[0] === 'string');
    chunk = new Buffer(chunk[0], chunk[1]);
    return TestWriter.prototype._write.call(this, chunk, cb);
  };

  var encodings =
      [ 'hex',
        'utf8',
        'utf-8',
        'ascii',
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
    var enc = encodings[ i % encodings.length ];
    chunk = new Buffer(chunk);
    tw.write(chunk.toString(enc), enc);
  });
  t.end();
});

test('write callbacks', function (t) {
  var callbacks = chunks.map(function(chunk, i) {
    return [i, function(er) {
      callbacks._called[i] = chunk;
    }];
  }).reduce(function(set, x) {
    set['callback-' + x[0]] = x[1];
    return set;
  }, {});
  callbacks._called = [];

  var tw = new TestWriter({
    lowWaterMark: 5,
    highWaterMark: 100
  });

  tw.on('finish', function() {
    t.same(tw.buffer, chunks, 'got chunks in the right order');
    t.same(callbacks._called, chunks, 'called all callbacks');
    t.end();
  });

  chunks.forEach(function(chunk, i) {
    tw.write(chunk, callbacks['callback-' + i]);
  });
  tw.end();
});
