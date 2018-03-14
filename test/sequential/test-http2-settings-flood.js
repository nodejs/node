'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const net = require('net');

const http2util = require('../common/http2');

// Test that settings flooding causes the session to be torn down

const kSettings = new http2util.SettingsFrame();

const server = http2.createServer();

let interval;

server.on('stream', common.mustNotCall());
server.on('session', common.mustCall((session) => {
  session.on('error', (e) => {
    assert.strictEqual(e.code, 'ERR_HTTP2_ERROR');
    assert(e.message.includes('Flooding was detected'));
    clearInterval(interval);
  });

  session.on('close', common.mustCall(() => {
    server.close();
  }));
}));

server.listen(0, common.mustCall(() => {
  const client = net.connect(server.address().port);

  // nghttp2 uses a limit of 10000 items in it's outbound queue.
  // If this number is exceeded, a flooding error is raised.
  // TODO(jasnell): Unfortunately, this test is inherently flaky because
  // it is entirely dependent on how quickly the server is able to handle
  // the inbound frames and whether those just happen to overflow nghttp2's
  // outbound queue. The threshold at which the flood error occurs can vary
  // from one system to another, and from one test run to another.
  client.on('connect', common.mustCall(() => {
    client.write(http2util.kClientMagic, () => {
      interval = setInterval(() => {
        for (let n = 0; n < 10000; n++)
          client.write(kSettings.data);
      }, 1);
    });
  }));

  // An error event may or may not be emitted, depending on operating system
  // and timing. We do not really care if one is emitted here or not, as the
  // error on the server side is what we are testing for. Do not make this
  // a common.mustCall() and there's no need to check the error details.
  client.on('error', () => {});
}));
