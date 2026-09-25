'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');
const net = require('net');
const http2util = require('../common/http2');

// Test that ping flooding causes the session to be torn down

const kSettings = new http2util.SettingsFrame();
const kPingAck = new http2util.PingFrame(true);

const server = http2.createServer();

server.on('stream', common.mustNotCall());
server.on('session', common.mustCall((session) => {
  session.on('error', common.expectsError({
    code: 'ERR_HTTP2_ERROR',
    message: 'Protocol error'
  }));
  session.on('close', common.mustCall(() => server.close()));
}));

server.listen(0, common.mustCall(() => {
  const client = net.connect(server.address().port);

  client.on('connect', common.mustCall(() => {
    client.write(http2util.kClientMagic, () => {
      client.write(kSettings.data);
      // Send an unsolicited ping ack
      client.write(kPingAck.data);
    });
  }));

  // An error event may or may not be emitted, depending on operating system
  // and timing. We do not really care if one is emitted here or not, as the
  // error on the server side is what we are testing for. Do not make this
  // a common.mustCall() and there's no need to check the error details.
  client.on('error', () => {});
}));
