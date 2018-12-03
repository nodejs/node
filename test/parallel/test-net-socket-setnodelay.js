'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

const truthyValues = [true, 1, 'true', {}, []];
const falseyValues = [false, 0, ''];
const genSetNoDelay = (desiredArg) => (enable) => {
  assert.strictEqual(enable, desiredArg);
};

// setNoDelay should default to true
let socket = new net.Socket({
  handle: {
    setNoDelay: common.mustCall(genSetNoDelay(true))
  }
});
socket.setNoDelay();

socket = new net.Socket({
  handle: {
    setNoDelay: common.mustCall(genSetNoDelay(true), truthyValues.length)
  }
});
truthyValues.forEach((testVal) => socket.setNoDelay(testVal));

socket = new net.Socket({
  handle: {
    setNoDelay: common.mustCall(genSetNoDelay(false), falseyValues.length)
  }
});
falseyValues.forEach((testVal) => socket.setNoDelay(testVal));

// If a handler doesn't have a setNoDelay function it shouldn't be called.
// In the case below, if it is called an exception will be thrown
socket = new net.Socket({
  handle: {
    setNoDelay: null
  }
});
const returned = socket.setNoDelay(true);
assert.ok(returned instanceof net.Socket);
