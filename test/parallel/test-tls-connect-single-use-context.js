'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

// tls.connect() builds a SecureContext per connection and marks it as single
// use, so that the underlying SSL_CTX is released as soon as the socket
// closes instead of when the JS wrapper happens to be garbage collected.
// Regression test for https://github.com/nodejs/node/issues/66002, where the
// flag was lost on its way to the context and nothing was ever closed.

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');

const server = tls.createServer({ key, cert }, (conn) => conn.end());

server.listen(0, common.mustCall(() => {
  connectWithOwnContext(common.mustCall(() => {
    connectWithSharedContext(common.mustCall(() => server.close()));
  }));
}));

// _destroySSL() runs from the immediate queue, after the 'close' event.
function afterDestroySSL(socket, fn) {
  socket.on('close', common.mustCall(() => setImmediate(fn)));
}

function connectWithOwnContext(done) {
  const socket = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
  }, common.mustCall(() => {
    const secureContext = socket.ssl._secureContext;
    assert.strictEqual(secureContext.singleUse, true);
    assert.notStrictEqual(secureContext.context, null);

    afterDestroySSL(socket, common.mustCall(() => {
      assert.strictEqual(secureContext.context, null);
      done();
    }));
  }));
}

function connectWithSharedContext(done) {
  // A context passed in by the user may outlive the connection, so it must
  // not be marked single use, and it must still work for the next socket.
  const secureContext = tls.createSecureContext();
  let remaining = 2;

  (function connectOnce() {
    const socket = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false,
      secureContext,
    }, common.mustCall(() => {
      assert.strictEqual(socket.ssl._secureContext, secureContext);
      assert.strictEqual(secureContext.singleUse, undefined);

      afterDestroySSL(socket, common.mustCall(() => {
        assert.notStrictEqual(secureContext.context, null);
        if (--remaining === 0) done();
        else connectOnce();
      }));
    }));
  })();
}
