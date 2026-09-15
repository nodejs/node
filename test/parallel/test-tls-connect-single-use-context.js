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
const { createMockedLookup } = require('../common/dns');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');

const server = tls.createServer({ key, cert }, (conn) => conn.end());

// Bound to a single address so that the addresses the last case retries
// through are refused rather than answered by this server.
server.listen(0, '127.0.0.1', common.mustCall(() => {
  connectWithOwnContext(common.mustCall(() => {
    connectWithSharedContext(common.mustCall(() => {
      connectAcrossHandleSwaps(common.mustCall(() => server.close()));
    }));
  }));
}));

// _destroySSL() runs from the immediate queue, after the 'close' event.
function afterDestroySSL(socket, fn) {
  socket.on('close', common.mustCall(() => setImmediate(fn)));
}

function connectWithOwnContext(done) {
  const socket = tls.connect({
    host: '127.0.0.1',
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
      host: '127.0.0.1',
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

function connectAcrossHandleSwaps(done) {
  // autoSelectFamily reinitializes the handle on every failed attempt, and the
  // successive TLSWraps share the socket's context. Releasing it with the old
  // handle leaves the next attempt without a context. Two failing addresses
  // are needed: the close happens on the first swap, and the next swap is what
  // trips over it.
  const socket = tls.connect({
    host: 'example.org',
    port: server.address().port,
    rejectUnauthorized: false,
    autoSelectFamily: true,
    autoSelectFamilyAttemptTimeout:
      common.defaultAutoSelectFamilyAttemptTimeout,
    lookup: createMockedLookup('::1', '127.0.0.2', '127.0.0.1'),
  }, common.mustCall(() => {
    // `ssl` is cleared while the handle is swapped, so read the context from
    // the handle the socket ended up with.
    const secureContext = socket._handle._secureContext;
    assert.strictEqual(secureContext.singleUse, true);
    assert.notStrictEqual(secureContext.context, null);

    afterDestroySSL(socket, common.mustCall(() => {
      assert.strictEqual(secureContext.context, null);
      done();
    }));
  }));
}
