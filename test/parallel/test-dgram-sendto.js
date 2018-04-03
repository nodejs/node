'use strict';
require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');

const errObj = {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError [ERR_INVALID_ARG_TYPE]',
  message: 'The "offset" argument must be of type number. Received type ' +
           'undefined'
};
assert.throws(() => socket.sendto(), errObj);

errObj.message = 'The "length" argument must be of type number. Received ' +
                 'type string';
assert.throws(
  () => socket.sendto('buffer', 1, 'offset', 'port', 'address', 'cb'),
  errObj);

errObj.message = 'The "offset" argument must be of type number. Received ' +
                 'type string';
assert.throws(
  () => socket.sendto('buffer', 'offset', 1, 'port', 'address', 'cb'),
  errObj);

errObj.message = 'The "address" argument must be of type string. Received ' +
                 'type boolean';
assert.throws(
  () => socket.sendto('buffer', 1, 1, 10, false, 'cb'),
  errObj);

errObj.message = 'The "port" argument must be of type number. Received ' +
                 'type boolean';
assert.throws(
  () => socket.sendto('buffer', 1, 1, false, 'address', 'cb'),
  errObj);
