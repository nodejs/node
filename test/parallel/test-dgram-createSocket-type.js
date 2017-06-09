'use strict';
require('../common');
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

// Error must be thrown with invalid types
invalidTypes.forEach((invalidType) => {
  assert.throws(() => {
    dgram.createSocket(invalidType);
  }, /Bad socket type specified/);
});

// Error must not be thrown with valid types
validTypes.forEach((validType) => {
  assert.doesNotThrow(() => {
    const socket = dgram.createSocket(validType);
    socket.close();
  });
});
