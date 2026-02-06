// Flags: --expose-internals
'use strict';

// Regression test for https://github.com/nodejs/node/issues/61304
// When the underlying socket is closed at the OS level without
// sending RST/FIN (e.g., network black hole), the HTTP/2 session
// enters a zombie state where it believes the connection is alive
// but the socket is actually dead. Subsequent write attempts should
// fail gracefully rather than crash with assertion failures.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const h2 = require('http2');
const { kSocket } = require('internal/http2/util');
const fixtures = require('../common/fixtures');

const server = h2.createSecureServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
});

server.on('stream', common.mustCall((stream) => {
  stream.respond({ ':status': 200 });
  stream.end('hello');
}));

server.listen(0, common.mustCall(() => {
  const client = h2.connect(`https://localhost:${server.address().port}`, {
    rejectUnauthorized: false
  });

  // Verify session eventually closes
  client.on('close', common.mustCall(() => {
    server.close();
  }));

  // First request to establish connection
  const req1 = client.request({ ':path': '/' });
  req1.on('response', common.mustCall());
  req1.on('data', () => {});
  req1.on('end', common.mustCall(() => {
    // Connection is established, now simulate network black hole
    // by destroying the underlying socket without proper close
    const socket = client[kSocket];

    // Verify session state before socket destruction
    assert.strictEqual(client.closed, false);
    assert.strictEqual(client.destroyed, false);

    // Destroy the socket to simulate OS-level connection loss
    // This mimics what happens when network drops packets without RST/FIN
    socket.destroy();

    // The session should handle this gracefully
    // Prior to fix: this would cause assertion failures in subsequent writes
    // After fix: session should close properly

    setImmediate(() => {
      // Try to send another request into the zombie session
      // With the fix, the session should close gracefully without crashing
      try {
        const req2 = client.request({ ':path': '/test' });
        // The request may or may not emit an error event,
        // but it should not receive a response
        req2.on('error', () => {
          // Acceptable: error event fires
        });
        req2.on('response', common.mustNotCall(
          'Should not receive response from zombie session'
        ));
        req2.end();
      } catch {
        // Also acceptable: synchronous error on request creation
      }

      // Force cleanup if session doesn't close naturally
      setTimeout(() => {
        if (!client.destroyed) {
          client.destroy();
        }
        if (server.listening) {
          server.close();
        }
      }, 1000);
    });
  }));

  req1.end();
}));
