'use strict';

// Regression test for TLSWrap use-after-free when destroySSL() is called
// while an underlying stream write is still in flight.
//
// EncOut() passes pointers into the enc_out_ BIO's internal buffer to the
// underlying stream via uv_write().  If the SSL context is freed before that
// write completes, libuv accesses freed memory (SIGSEGV).
//
// The GC weak callback path triggers the same Destroy() code when a
// TLSSocket with pending writes is collected without an explicit destroy().

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const tls = require('node:tls');

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
}, (socket) => {
  // Do not read — TCP backpressure keeps client writes pending in the
  // TLSWrap's underlying stream so write_size_ stays non-zero.
  socket.pause();
});

server.listen(0, common.mustCall(() => {
  const { port } = server.address();
  let remaining = 10;

  for (let i = 0; i < 10; i++) {
    const socket = tls.connect(
      { port, host: '127.0.0.1', rejectUnauthorized: false },
      common.mustCall(() => {
        // Queue a write large enough to stay pending (server never reads).
        socket.write(Buffer.alloc(1024 * 1024));

        // Simulate the GC weak callback path: free the SSL context while
        // the underlying stream write is still in flight.
        if (socket._handle && socket._handle.destroySSL) {
          socket._handle.destroySSL();
        }

        setTimeout(() => {
          socket.destroy();
          if (--remaining === 0) {
            server.close();
          }
        }, 50);
      }),
    );
    socket.on('error', () => {});
  }
}));
