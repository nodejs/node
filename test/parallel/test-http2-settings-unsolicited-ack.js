'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const net = require('net');
const http2util = require('../common/http2');
const Countdown = require('../common/countdown');

// Test that an unsolicited settings ack is ignored.

const kSettings = new http2util.SettingsFrame();
const kSettingsAck = new http2util.SettingsFrame(true);

const server = http2.createServer();
let client;

const countdown = new Countdown(3, () => {
  client.destroy();
  server.close();
});

server.on('stream', common.mustNotCall());
server.on('session', common.mustCall((session) => {
  session.on('remoteSettings', common.mustCall(() => countdown.dec()));
}));

server.listen(0, common.mustCall(() => {
  client = net.connect(server.address().port);

  // Ensures that the clients settings frames are not sent until the
  // servers are received, so that the first ack is actually expected.
  client.once('data', (chunk) => {
    // The very first chunk of data we get from the server should
    // be a settings frame.
    assert.deepStrictEqual(chunk.slice(0, 9), kSettings.data);
    // The first ack is expected.
    client.write(kSettingsAck.data, () => countdown.dec());
    // The second one is not and will be ignored.
    client.write(kSettingsAck.data, () => countdown.dec());
  });

  client.on('connect', common.mustCall(() => {
    client.write(http2util.kClientMagic);
    client.write(kSettings.data);
  }));
}));
