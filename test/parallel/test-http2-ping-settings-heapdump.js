'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');
const v8 = require('v8');

// Regression test for https://github.com/nodejs/node/issues/28088:
// Verify that Http2Settings and Http2Ping objects don't reference the session
// after it is destroyed, either because they are detached from it or have been
// destroyed themselves.

for (const variant of ['ping', 'settings']) {
  const server = http2.createServer();
  server.on('session', common.mustCall((session) => {
    if (variant === 'ping') {
      session.ping(common.expectsError({
        code: 'ERR_HTTP2_PING_CANCEL'
      }));
    } else {
      session.settings(undefined, common.mustNotCall());
    }

    session.on('close', common.mustCall(() => {
      v8.getHeapSnapshot().resume();
      server.close();
    }));
    session.destroy();
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`,
                                 common.mustCall());
    client.on('error', (err) => {
      // We destroy the session so it's possible to get ECONNRESET here.
      if (err.code !== 'ECONNRESET')
        throw err;
    });
  }));
}
