'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const PORT = 12345;

const client = dgram.createSocket('udp4');
client.connect(PORT, common.mustCall(() => {
  const remoteAddr = client.remoteAddress();
  assert.strictEqual(remoteAddr.port, PORT);
  assert.throws(() => {
    client.connect(PORT, common.mustNotCall());
  }, {
    name: 'Error',
    code: 'ERR_SOCKET_DGRAM_IS_CONNECTED'
  });

  client.disconnect();
  assert.throws(() => {
    client.disconnect();
  }, {
    name: 'Error',
    code: 'ERR_SOCKET_DGRAM_NOT_CONNECTED'
  });

  assert.throws(() => {
    client.remoteAddress();
  }, {
    name: 'Error',
    code: 'ERR_SOCKET_DGRAM_NOT_CONNECTED'
  });

  client.once('connect', common.mustCall(() => client.close()));
  client.connect(PORT);
}));

assert.throws(() => {
  client.connect(PORT);
}, {
  name: 'Error',
  code: 'ERR_SOCKET_DGRAM_IS_CONNECTED'
});

assert.throws(() => {
  client.disconnect();
}, {
  name: 'Error',
  code: 'ERR_SOCKET_DGRAM_NOT_CONNECTED'
});

[ 0, null, 78960, undefined ].forEach((port) => {
  assert.throws(() => {
    client.connect(port);
  }, {
    name: 'RangeError',
    code: 'ERR_SOCKET_BAD_PORT'
  });
});
