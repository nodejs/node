'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const buf = Buffer.from('test');

const onMessage = common.mustCall((err, bytes) => {
  assert.ifError(err);
  assert.strictEqual(bytes, buf.length);
}, 6);

const expectedError = { code: 'ERR_INVALID_ARG_TYPE',
                        type: TypeError,
                        message:
  /^The "address" argument must be one of type string or falsy$/
};

const client = dgram.createSocket('udp4').bind(0, () => {
  const port = client.address().port;

  // valid address: false
  client.send(buf, port, false, onMessage);

  // valid address: empty string
  client.send(buf, port, '', onMessage);

  // valid address: null
  client.send(buf, port, null, onMessage);

  // valid address: 0
  client.send(buf, port, 0, onMessage);

  // valid address: undefined
  client.send(buf, port, undefined, onMessage);

  // valid address: not provided
  client.send(buf, port, onMessage);

  // invalid address: object
  assert.throws(() => {
    client.send(buf, port, []);
  }, common.expectsError(expectedError));

  // invalid address: nonzero number
  assert.throws(() => {
    client.send(buf, port, 1);
  }, common.expectsError(expectedError));

  // invalid address: true
  assert.throws(() => {
    client.send(buf, port, true);
  }, common.expectsError(expectedError));
});

client.unref();
