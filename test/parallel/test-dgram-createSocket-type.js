'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const invalidTypes = [
  'test',
  ['udp4'],
  new String('udp4'),
  1,
  {},
  true,
  false,
  null,
  undefined
];
const validTypes = [
  'udp4',
  'udp6',
  { type: 'udp4' },
  { type: 'udp6' }
];
const errMessage = /^Bad socket type specified\. Valid types are: udp4, udp6$/;

// Error must be thrown with invalid types
invalidTypes.forEach((invalidType) => {
  assert.throws(() => {
    dgram.createSocket(invalidType);
  }, common.expectsError({
    code: 'ERR_SOCKET_BAD_TYPE',
    type: TypeError,
    message: errMessage
  }));
});

// Error must not be thrown with valid types
validTypes.forEach((validType) => {
  assert.doesNotThrow(() => {
    const socket = dgram.createSocket(validType);
    socket.close();
  });
});

// Ensure buffer sizes can be set
{
  const socket = dgram.createSocket({ type: 'udp4',
                                      recvBufferSize: 1300,
                                      sendBufferSize: 1800,
                                    });

  socket.bind(common.mustCall(() => {
    // note: linux will double the buffer size
    assert.ok(socket.getRecvBufferSize() === 1300 ||
              socket.getRecvBufferSize() === 2600,
              "SO_RCVBUF not 1300 or 2600");
    assert.ok(socket.getSendBufferSize() === 1800 ||
              socket.getSendBufferSize() === 3600,
              "SO_SNDBUF not 1800 or 3600");
    socket.close();
  }));
}
