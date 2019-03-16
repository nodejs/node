'use strict';

const common = require('../common');
const { addresses } = require('../common/internet');
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
    name: 'Error [ERR_SOCKET_DGRAM_IS_CONNECTED]',
    message: 'Already connected',
    code: 'ERR_SOCKET_DGRAM_IS_CONNECTED'
  });

  client.disconnect();
  assert.throws(() => {
    client.disconnect();
  }, {
    name: 'Error [ERR_SOCKET_DGRAM_NOT_CONNECTED]',
    message: 'Not connected',
    code: 'ERR_SOCKET_DGRAM_NOT_CONNECTED'
  });

  assert.throws(() => {
    client.remoteAddress();
  }, {
    name: 'Error [ERR_SOCKET_DGRAM_NOT_CONNECTED]',
    message: 'Not connected',
    code: 'ERR_SOCKET_DGRAM_NOT_CONNECTED'
  });

  client.connect(PORT, addresses.INVALID_HOST, common.mustCall((err) => {
    assert.ok(err.code === 'ENOTFOUND' || err.code === 'EAI_AGAIN');

    client.once('error', common.mustCall((err) => {
      assert.ok(err.code === 'ENOTFOUND' || err.code === 'EAI_AGAIN');
      client.once('connect', common.mustCall(() => client.close()));
      client.connect(PORT);
    }));

    client.connect(PORT, addresses.INVALID_HOST);
  }));
}));

assert.throws(() => {
  client.connect(PORT);
}, {
  name: 'Error [ERR_SOCKET_DGRAM_IS_CONNECTED]',
  message: 'Already connected',
  code: 'ERR_SOCKET_DGRAM_IS_CONNECTED'
});

assert.throws(() => {
  client.disconnect();
}, {
  name: 'Error [ERR_SOCKET_DGRAM_NOT_CONNECTED]',
  message: 'Not connected',
  code: 'ERR_SOCKET_DGRAM_NOT_CONNECTED'
});

[ 0, null, 78960, undefined ].forEach((port) => {
  assert.throws(() => {
    client.connect(port);
  }, {
    name: 'RangeError [ERR_SOCKET_BAD_PORT]',
    message: /^Port should be >= 0 and < 65536/,
    code: 'ERR_SOCKET_BAD_PORT'
  });
});
