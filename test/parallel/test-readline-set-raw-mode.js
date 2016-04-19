'use strict';
require('../common');
var assert = require('assert');
var readline = require('readline');
var Stream = require('stream');

var stream = new Stream();
var expectedRawMode = true;
var rawModeCalled = false;
var resumeCalled = false;
var pauseCalled = false;

stream.setRawMode = function(mode) {
  rawModeCalled = true;
  assert.equal(mode, expectedRawMode);
};
stream.resume = function() {
  resumeCalled = true;
};
stream.pause = function() {
  pauseCalled = true;
};

// when the "readline" starts in "terminal" mode,
// then setRawMode(true) should be called
var rli = readline.createInterface({
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
assert.equal(stream.listeners('data').length, 1);
