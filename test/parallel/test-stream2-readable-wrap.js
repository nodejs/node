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

var common = require('../common');
var assert = require('assert');

var Readable = require('_stream_readable');
var Writable = require('_stream_writable');
var EE = require('events').EventEmitter;

var testRuns = 0, completedRuns = 0;
function runTest(highWaterMark, objectMode, produce) {
  testRuns++;

  var old = new EE;
  var r = new Readable({ highWaterMark: highWaterMark, objectMode: objectMode });
  assert.equal(r, r.wrap(old));

  var ended = false;
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

  var flowing;
  var chunks = 10;
  var oldEnded = false;
  var expected = [];
  function flow() {
    flowing = true;
    while (flowing && chunks-- > 0) {
      var item = produce();
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

  var w = new Writable({ highWaterMark: highWaterMark * 2, objectMode: objectMode });
  var written = [];
  w._write = function(chunk, encoding, cb) {
    console.log('_write', chunk);
    written.push(chunk);
    setTimeout(cb);
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
    assert.deepEqual(written, expected);
  }
}

runTest(100, false, function(){ return new Buffer(100); });
runTest(10, false, function(){ return new Buffer('xxxxxxxxxx'); });
runTest(1, true, function(){ return { foo: 'bar' }; });

var objectChunks = [ 5, 'a', false, 0, '', 'xyz', { x: 4 }, 7, [], 555 ];
runTest(1, true, function(){ return objectChunks.shift() });

process.on('exit', function() {
  assert.equal(testRuns, completedRuns);
  console.log('ok');
});
