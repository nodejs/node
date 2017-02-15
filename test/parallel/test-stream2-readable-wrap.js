'use strict';
const common = require('../common');
const assert = require('assert');
const Readable = require('_stream_readable');
const Writable = require('_stream_writable');
const EE = require('events').EventEmitter;

function runTest(highWaterMark, objectMode, produce) {

  const old = new EE();
  const r = new Readable({ highWaterMark: highWaterMark,
                           objectMode: objectMode });
  assert.strictEqual(r, r.wrap(old));

  r.on('end', common.mustCall(function() {}));

  old.pause = function() {
    old.emit('pause');
    flowing = false;
  };

  old.resume = function() {
    old.emit('resume');
    flow();
  };

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
                           objectMode: objectMode });
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
