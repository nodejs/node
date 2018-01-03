'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');
const net = require('net');
const http2util = require('../common/http2');

// Test that ping flooding causes the session to be torn down

const kSettings = new http2util.SettingsFrame();
const kPing = new http2util.PingFrame();

const server = http2.createServer();

server.on('stream', common.mustNotCall());
server.on('session', common.mustCall((session) => {
  session.on('error', common.expectsError({
    code: 'ERR_HTTP2_ERROR',
    message:
      'Flooding was detected in this HTTP/2 session, and it must be closed'
  }));
  session.on('close', common.mustCall(() => {
    server.close();
  }));
}));

server.listen(0, common.mustCall(() => {
  const client = net.connect(server.address().port);

  // nghttp2 uses a limit of 10000 items in it's outbound queue.
  // If this number is exceeded, a flooding error is raised. Set
  // this lim higher to account for the ones that nghttp2 is
  // successfully able to respond to.
  // TODO(jasnell): Unfortunately, this test is inherently flaky because
  // it is entirely dependent on how quickly the server is able to handle
  // the inbound frames and whether those just happen to overflow nghttp2's
  // outbound queue. The threshold at which the flood error occurs can vary
  // from one system to another, and from one test run to another.
  client.on('connect', common.mustCall(() => {
    client.write(http2util.kClientMagic, () => {
      client.write(kSettings.data, () => {
        for (let n = 0; n < 35000; n++)
          client.write(kPing.data);
      });
    });
  }));

  // An error event may or may not be emitted, depending on operating system
  // and timing. We do not really care if one is emitted here or not, as the
  // error on the server side is what we are testing for. Do not make this
  // a common.mustCall() and there's no need to check the error details.
  client.on('error', () => {});
}));
