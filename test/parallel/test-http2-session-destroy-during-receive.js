'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const http2 = require('http2');

// Regression test for closing a session while nghttp2 is processing several
// streams from the same input buffer. No stream created after the close can be
// exposed to JavaScript, so delivering its DATA would call a missing onread.
const server = http2.createSecureServer({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem')
});

server.on('stream', common.mustCallAtLeast((stream) => {
  stream.on('error', () => {});
  stream.session.destroy();
}, 1));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`https://localhost:${server.address().port}`, {
    rejectUnauthorized: false
  });
  client.on('error', () => {});
  client.on('close', common.mustCall(() => server.close()));

  client.on('remoteSettings', common.mustCall(() => {
    for (let i = 0; i < 8; i++) {
      const stream = client.request({
        ':method': 'POST',
        ':path': `/${i}`
      });
      stream.on('error', () => {});
      stream.resume();
      stream.end(Buffer.alloc(512));
    }
  }));
}));
