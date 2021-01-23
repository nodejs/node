'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');

const server = http2.createServer({ maxSettings: 1 });

// TODO(@jasnell): There is still a session event
// emitted on the server side but it will be destroyed
// immediately after creation and there will be no
// stream created.
server.on('session', common.mustCall((session) => {
  session.on('stream', common.mustNotCall());
  session.on('remoteSettings', common.mustNotCall());
}));
server.on('stream', common.mustNotCall());

server.listen(0, common.mustCall(() => {
  // Specify two settings entries when a max of 1 is allowed.
  // Connection should error immediately.
  const client = http2.connect(
    `http://localhost:${server.address().port}`, {
      settings: {
        // The actual settings values do not matter.
        headerTableSize: 1000,
        enablePush: false,
      },
    });

  client.on('error', common.mustCall(() => {
    server.close();
  }));
}));
