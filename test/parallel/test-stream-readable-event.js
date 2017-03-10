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

const Readable = require('stream').Readable;

{
  // First test, not reading when the readable is added.
  // make sure that on('readable', ...) triggers a readable event.
  const r = new Readable({
    highWaterMark: 3
  });

  r._read = common.mustNotCall();

  // This triggers a 'readable' event, which is lost.
  r.push(Buffer.from('blerg'));

  setTimeout(function() {
    // we're testing what we think we are
    assert(!r._readableState.reading);
    r.on('readable', common.mustCall(function() {}));
  }, 1);
}

{
  // second test, make sure that readable is re-emitted if there's
  // already a length, while it IS reading.

  const r = new Readable({
    highWaterMark: 3
  });

  r._read = common.mustCall(function(n) {});

  // This triggers a 'readable' event, which is lost.
  r.push(Buffer.from('bl'));

  setTimeout(function() {
    // assert we're testing what we think we are
    assert(r._readableState.reading);
    r.on('readable', common.mustCall(function() {}));
  }, 1);
}

{
  // Third test, not reading when the stream has not passed
  // the highWaterMark but *has* reached EOF.
  const r = new Readable({
    highWaterMark: 30
  });

  r._read = common.mustNotCall();

  // This triggers a 'readable' event, which is lost.
  r.push(Buffer.from('blerg'));
  r.push(null);

  setTimeout(function() {
    // assert we're testing what we think we are
    assert(!r._readableState.reading);
    r.on('readable', common.mustCall(function() {}));
  }, 1);
}
