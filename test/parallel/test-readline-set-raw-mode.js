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
