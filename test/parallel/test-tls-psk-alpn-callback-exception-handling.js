'use strict';

// This test verifies that exceptions in pskCallback and ALPNCallback are
// properly routed through tlsClientError instead of becoming uncaught
// exceptions. This is a regression test for a vulnerability where callback
// validation errors would bypass all standard TLS error handlers.
//
// The vulnerability allows remote attackers to crash TLS servers or cause
// resource exhaustion (file descriptor leaks) when pskCallback or ALPNCallback
// throw exceptions during validation.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { describe, it } = require('node:test');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const CIPHERS = 'PSK+HIGH';
const TEST_TIMEOUT = 5000;

// Helper to create a promise that rejects on uncaughtException or timeout
function createTestPromise() {
  const { promise, resolve, reject } = Promise.withResolvers();
  let settled = false;

  const cleanup = () => {
    if (!settled) {
      settled = true;
      process.removeListener('uncaughtException', onUncaught);
      clearTimeout(timeout);
    }
  };

  const onUncaught = (err) => {
    cleanup();
    reject(new Error(
      `Uncaught exception instead of tlsClientError: ${err.code || err.message}`
    ));
  };

  const timeout = setTimeout(() => {
    cleanup();
    reject(new Error('Test timed out - tlsClientError was not emitted'));
  }, TEST_TIMEOUT);

  process.on('uncaughtException', onUncaught);

  return {
    resolve: (value) => {
      cleanup();
      resolve(value);
    },
    reject: (err) => {
      cleanup();
      reject(err);
    },
    promise,
  };
}

describe('TLS callback exception handling', () => {

  // Test 1: PSK server callback returning invalid type should emit tlsClientError
  it('pskCallback returning invalid type emits tlsClientError', async (t) => {
    const server = tls.createServer({
      ciphers: CIPHERS,
      pskCallback: () => {
        // Return invalid type (string instead of object/Buffer)
        return 'invalid-should-be-object-or-buffer';
      },
      pskIdentityHint: 'test-hint',
    });

    t.after(() => server.close());

    const { promise, resolve, reject } = createTestPromise();

    server.on('tlsClientError', common.mustCall((err, socket) => {
      try {
        assert.ok(err instanceof Error);
        assert.strictEqual(err.code, 'ERR_INVALID_ARG_TYPE');
        socket.destroy();
        resolve();
      } catch (e) {
        reject(e);
      }
    }));

    server.on('secureConnection', common.mustNotCall(() => {
      reject(new Error('secureConnection should not fire'));
    }));

    await new Promise((res) => server.listen(0, res));

    const client = tls.connect({
      port: server.address().port,
      host: '127.0.0.1',
      ciphers: CIPHERS,
      checkServerIdentity: () => {},
      pskCallback: () => ({
        psk: Buffer.alloc(32),
        identity: 'test-identity',
      }),
    });

    client.on('error', () => {});

    await promise;
  });

  // Test 2: PSK server callback throwing should emit tlsClientError
  it('pskCallback throwing emits tlsClientError', async (t) => {
    const server = tls.createServer({
      ciphers: CIPHERS,
      pskCallback: () => {
        throw new Error('Intentional callback error');
      },
      pskIdentityHint: 'test-hint',
    });

    t.after(() => server.close());

    const { promise, resolve, reject } = createTestPromise();

    server.on('tlsClientError', common.mustCall((err, socket) => {
      try {
        assert.ok(err instanceof Error);
        assert.strictEqual(err.message, 'Intentional callback error');
        socket.destroy();
        resolve();
      } catch (e) {
        reject(e);
      }
    }));

    server.on('secureConnection', common.mustNotCall(() => {
      reject(new Error('secureConnection should not fire'));
    }));

    await new Promise((res) => server.listen(0, res));

    const client = tls.connect({
      port: server.address().port,
      host: '127.0.0.1',
      ciphers: CIPHERS,
      checkServerIdentity: () => {},
      pskCallback: () => ({
        psk: Buffer.alloc(32),
        identity: 'test-identity',
      }),
    });

    client.on('error', () => {});

    await promise;
  });

  // Test 3: ALPN callback returning non-matching protocol should emit tlsClientError
  it('ALPNCallback returning invalid result emits tlsClientError', async (t) => {
    const server = tls.createServer({
      key: fixtures.readKey('agent2-key.pem'),
      cert: fixtures.readKey('agent2-cert.pem'),
      ALPNCallback: () => {
        // Return a protocol not in the client's list
        return 'invalid-protocol-not-in-list';
      },
    });

    t.after(() => server.close());

    const { promise, resolve, reject } = createTestPromise();

    server.on('tlsClientError', common.mustCall((err, socket) => {
      try {
        assert.ok(err instanceof Error);
        assert.strictEqual(err.code, 'ERR_TLS_ALPN_CALLBACK_INVALID_RESULT');
        socket.destroy();
        resolve();
      } catch (e) {
        reject(e);
      }
    }));

    server.on('secureConnection', common.mustNotCall(() => {
      reject(new Error('secureConnection should not fire'));
    }));

    await new Promise((res) => server.listen(0, res));

    const client = tls.connect({
      port: server.address().port,
      host: '127.0.0.1',
      rejectUnauthorized: false,
      ALPNProtocols: ['http/1.1', 'h2'],
    });

    client.on('error', () => {});

    await promise;
  });

  // Test 4: ALPN callback throwing should emit tlsClientError
  it('ALPNCallback throwing emits tlsClientError', async (t) => {
    const server = tls.createServer({
      key: fixtures.readKey('agent2-key.pem'),
      cert: fixtures.readKey('agent2-cert.pem'),
      ALPNCallback: () => {
        throw new Error('Intentional ALPN callback error');
      },
    });

    t.after(() => server.close());

    const { promise, resolve, reject } = createTestPromise();

    server.on('tlsClientError', common.mustCall((err, socket) => {
      try {
        assert.ok(err instanceof Error);
        assert.strictEqual(err.message, 'Intentional ALPN callback error');
        socket.destroy();
        resolve();
      } catch (e) {
        reject(e);
      }
    }));

    server.on('secureConnection', common.mustNotCall(() => {
      reject(new Error('secureConnection should not fire'));
    }));
    await new Promise((res) => server.listen(0, res));

    const client = tls.connect({
      port: server.address().port,
      host: '127.0.0.1',
      rejectUnauthorized: false,
      ALPNProtocols: ['http/1.1'],
    });

    client.on('error', () => {});

    await promise;
  });

  // Test 5: PSK client callback returning invalid type should emit error event
  it('client pskCallback returning invalid type emits error', async (t) => {
    const PSK = Buffer.alloc(32);

    const server = tls.createServer({
      ciphers: CIPHERS,
      pskCallback: () => PSK,
      pskIdentityHint: 'test-hint',
    });

    t.after(() => server.close());

    const { promise, resolve, reject } = createTestPromise();

    server.on('secureConnection', common.mustNotCall(() => {
      reject(new Error('secureConnection should not fire'));
    }));

    await new Promise((res) => server.listen(0, res));

    const client = tls.connect({
      port: server.address().port,
      host: '127.0.0.1',
      ciphers: CIPHERS,
      checkServerIdentity: () => {},
      pskCallback: () => {
        // Return invalid type - should cause validation error
        return 'invalid-should-be-object';
      },
    });

    client.on('error', common.mustCall((err) => {
      try {
        assert.ok(err instanceof Error);
        assert.strictEqual(err.code, 'ERR_INVALID_ARG_TYPE');
        resolve();
      } catch (e) {
        reject(e);
      }
    }));

    await promise;
  });

  // Test 6: PSK client callback throwing should emit error event
  it('client pskCallback throwing emits error', async (t) => {
    const PSK = Buffer.alloc(32);

    const server = tls.createServer({
      ciphers: CIPHERS,
      pskCallback: () => PSK,
      pskIdentityHint: 'test-hint',
    });

    t.after(() => server.close());

    const { promise, resolve, reject } = createTestPromise();

    server.on('secureConnection', common.mustNotCall(() => {
      reject(new Error('secureConnection should not fire'));
    }));

    await new Promise((res) => server.listen(0, res));

    const client = tls.connect({
      port: server.address().port,
      host: '127.0.0.1',
      ciphers: CIPHERS,
      checkServerIdentity: () => {},
      pskCallback: () => {
        throw new Error('Intentional client PSK callback error');
      },
    });

    client.on('error', common.mustCall((err) => {
      try {
        assert.ok(err instanceof Error);
        assert.strictEqual(err.message, 'Intentional client PSK callback error');
        resolve();
      } catch (e) {
        reject(e);
      }
    }));

    await promise;
  });
});
