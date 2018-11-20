'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const buf = Buffer.from('test');

const onMessage = common.mustCall((err, bytes) => {
  assert.ifError(err);
  assert.strictEqual(bytes, buf.length);
}, 6);

const client = dgram.createSocket('udp4').bind(0, () => {
  const port = client.address().port;

  // Check valid addresses
  [false, '', null, 0, undefined].forEach((address) => {
    client.send(buf, port, address, onMessage);
  });

  // Valid address: not provided
  client.send(buf, port, onMessage);

  // Check invalid addresses
  [[], 1, true].forEach((invalidInput) => {
    const expectedError = {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError [ERR_INVALID_ARG_TYPE]',
      message: 'The "address" argument must be one of type string or falsy. ' +
               `Received type ${typeof invalidInput}`
    };
    assert.throws(() => client.send(buf, port, invalidInput), expectedError);
  });
});

client.unref();
