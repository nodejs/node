'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const buf = Buffer.from('test');

const defaultCases = ['', null, undefined];

const onMessage = common.mustSucceed((bytes) => {
  assert.strictEqual(bytes, buf.length);
}, defaultCases.length + 1);

const client = dgram.createSocket('udp4').bind(0, () => {
  const port = client.address().port;

  // Check valid addresses
  defaultCases.forEach((address) => {
    client.send(buf, port, address, onMessage);
  });

  // Valid address: not provided
  client.send(buf, port, onMessage);

  // Check invalid addresses
  [
    [],
    0,
    1,
    true,
    false,
    0n,
    1n,
    {},
    Symbol(),
  ].forEach((invalidInput) => {
    const expectedError = {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "address" argument must be of type string.' +
               `${common.invalidArgTypeHelper(invalidInput)}`
    };
    assert.throws(() => client.send(buf, port, invalidInput), expectedError);
  });
});

client.unref();
