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

var Readable = require('stream').Readable;

(function first() {
  // First test, not reading when the readable is added.
  // make sure that read(0) triggers a readable event.
  var r = new Readable({
    highWaterMark: 3
  });

  r._read = function(n) {
    r.push(new Buffer(new Array(n + 1).join('x')));
  };

  // This triggers a 'readable' event, which is lost.
  r.push(new Buffer('blerg'));

  var caughtReadable = false;
  setTimeout(function() {
    r.on('readable', function() {
      caughtReadable = true;
    });
  });

  process.on('exit', function() {
    assert(caughtReadable);
    console.log('ok 1');
  });
})();

(function second() {
  // second test, make sure that readable is re-emitted if there's
  // already a length, while it IS reading.

  var r = new Readable({
    highWaterMark: 3
  });

  r._read = function(n) {
    setTimeout(function() {
      r.push(new Buffer(new Array(n + 1).join('x')));
    });
  };

  // This triggers a 'readable' event, which is lost.
  r.push(new Buffer('blerg'));

  var caughtReadable = false;
  process.nextTick(function() {
    r.on('readable', function() {
      caughtReadable = true;
    });
  });

  process.on('exit', function() {
    assert(caughtReadable);
    console.log('ok 2');
  });
})();

(function third() {
  // Third test, not reading when the stream has not passed
  // the highWaterMark but *has* reached EOF.
  var r = new Readable({
    highWaterMark: 30
  });

  // This triggers a 'readable' event, which is lost.
  r.push(new Buffer('blerg'));
  r.push(null);

  var caughtReadable = false;
  setTimeout(function() {
    r.on('readable', function() {
      caughtReadable = true;
    });
  });

  process.on('exit', function() {
    assert(caughtReadable);
    console.log('ok 3');
  });
})();
