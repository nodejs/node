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
const assert = require('assert');
const readline = require('readline');
const Stream = require('stream');

const stream = new Stream();
let expectedRawMode = true;
let rawModeCalled = false;
let resumeCalled = false;
let pauseCalled = false;

stream.setRawMode = function(mode) {
  rawModeCalled = true;
  assert.strictEqual(mode, expectedRawMode);
};
stream.resume = function() {
  resumeCalled = true;
};
stream.pause = function() {
  pauseCalled = true;
};

// when the "readline" starts in "terminal" mode,
// then setRawMode(true) should be called
const rli = readline.createInterface({
  input: stream,
  output: stream,
  terminal: true
});
assert(rli.terminal);
assert(rawModeCalled);
assert(resumeCalled);
assert(!pauseCalled);


// pause() should call *not* call setRawMode()
rawModeCalled = false;
resumeCalled = false;
pauseCalled = false;
rli.pause();
assert(!rawModeCalled);
assert(!resumeCalled);
assert(pauseCalled);


// resume() should *not* call setRawMode()
rawModeCalled = false;
resumeCalled = false;
pauseCalled = false;
rli.resume();
assert(!rawModeCalled);
assert(resumeCalled);
assert(!pauseCalled);


// close() should call setRawMode(false)
expectedRawMode = false;
rawModeCalled = false;
resumeCalled = false;
pauseCalled = false;
rli.close();
assert(rawModeCalled);
assert(!resumeCalled);
assert(pauseCalled);

assert.deepStrictEqual(stream.listeners('keypress'), []);
// one data listener for the keypress events.
assert.strictEqual(stream.listeners('data').length, 1);
