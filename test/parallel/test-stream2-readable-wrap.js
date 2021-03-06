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
const assert = require('assert');
const { Readable, Writable } = require('stream');
const EE = require('events').EventEmitter;

function runTest(highWaterMark, objectMode, produce) {

  const old = new EE();
  const r = new Readable({ highWaterMark, objectMode });
  assert.strictEqual(r, r.wrap(old));

  r.on('end', common.mustCall());

  old.pause = function() {
    old.emit('pause');
    flowing = false;
  };

  old.resume = function() {
    old.emit('resume');
    flow();
  };

  // Make sure pause is only emitted once.
  let pausing = false;
  r.on('pause', () => {
    assert.strictEqual(pausing, false);
    pausing = true;
    process.nextTick(() => {
      pausing = false;
    });
  });

  let flowing;
  let chunks = 10;
  let oldEnded = false;
  const expected = [];
  function flow() {
    flowing = true;
    while (flowing && chunks-- > 0) {
      const item = produce();
      expected.push(item);
      old.emit('data', item);
    }
    if (chunks <= 0) {
      oldEnded = true;
      old.emit('end');
    }
  }

  const w = new Writable({ highWaterMark: highWaterMark * 2,
                           objectMode });
  const written = [];
  w._write = function(chunk, encoding, cb) {
    written.push(chunk);
    setTimeout(cb, 1);
  };

  w.on('finish', common.mustCall(function() {
    performAsserts();
  }));

  r.pipe(w);

  flow();

  function performAsserts() {
    assert(oldEnded);
    assert.deepStrictEqual(written, expected);
  }
}

runTest(100, false, function() { return Buffer.allocUnsafe(100); });
runTest(10, false, function() { return Buffer.from('xxxxxxxxxx'); });
runTest(1, true, function() { return { foo: 'bar' }; });

const objectChunks = [ 5, 'a', false, 0, '', 'xyz', { x: 4 }, 7, [], 555 ];
runTest(1, true, function() { return objectChunks.shift(); });
