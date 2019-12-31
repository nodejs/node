'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const socket = net.Stream({ highWaterMark: 0 });

// Make sure that anything besides a buffer or a string throws.
assert.throws(() => socket.write(null),
              {
                code: 'ERR_STREAM_NULL_VALUES',
                name: 'TypeError',
                message: 'May not write null values to stream'
              });
[
  true,
  false,
  undefined,
  1,
  1.0,
  +Infinity,
  -Infinity,
  [],
  {}
].forEach((value) => {
  // We need to check the callback since 'error' will only
  // be emitted once per instance.
  socket.write(value, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "chunk" argument must be of type string or an instance of ' +
              `Buffer or Uint8Array.${common.invalidArgTypeHelper(value)}`
  }));
});
