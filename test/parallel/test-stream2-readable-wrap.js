'use strict';
require('../common');
const assert = require('assert');

const Readable = require('_stream_readable');
const Writable = require('_stream_writable');
const EE = require('events').EventEmitter;

let testRuns = 0, completedRuns = 0;
function runTest(highWaterMark, objectMode, produce) {
  testRuns++;

  const old = new EE();
  const r = new Readable({ highWaterMark: highWaterMark,
                           objectMode: objectMode });
  assert.strictEqual(r, r.wrap(old));

  let ended = false;
  r.on('end', function() {
    ended = true;
  });

  old.pause = function() {
    console.error('old.pause()');
    old.emit('pause');
    flowing = false;
  };

  old.resume = function() {
    console.error('old.resume()');
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
      console.log('old.emit', chunks, flowing);
      old.emit('data', item);
      console.log('after emit', chunks, flowing);
    }
    if (chunks <= 0) {
      oldEnded = true;
      console.log('old end', chunks, flowing);
      old.emit('end');
    }
  }

  const w = new Writable({ highWaterMark: highWaterMark * 2,
                           objectMode: objectMode });
  const written = [];
  w._write = function(chunk, encoding, cb) {
    console.log('_write', chunk);
    written.push(chunk);
    setTimeout(cb, 1);
  };

  w.on('finish', function() {
    completedRuns++;
    performAsserts();
  });

  r.pipe(w);

  flow();

  function performAsserts() {
    assert(ended);
    assert(oldEnded);
    assert.deepStrictEqual(written, expected);
  }
}

runTest(100, false, function() { return Buffer.allocUnsafe(100); });
runTest(10, false, function() { return Buffer.from('xxxxxxxxxx'); });
runTest(1, true, function() { return { foo: 'bar' }; });

const objectChunks = [ 5, 'a', false, 0, '', 'xyz', { x: 4 }, 7, [], 555 ];
runTest(1, true, function() { return objectChunks.shift(); });

process.on('exit', function() {
  assert.strictEqual(testRuns, completedRuns);
  console.log('ok');
});
