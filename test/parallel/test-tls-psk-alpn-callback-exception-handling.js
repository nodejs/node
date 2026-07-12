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

if (process.features.openssl_is_boringssl) {
  require('../common/boringssl').testPskTls13Unsupported();
  return;
}

const assert = require('assert');
const { describe, it } = require('node:test');
const https = require('node:https');
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
    server.on('secureConnection', common.mustNotCall('secureConnection listener'));

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

    client.on('error', common.mustCall());

    await promise;
  });

  it('newSession synchronous error emits error', async (t) => {
    const server = tls.createServer({
      key: fixtures.readKey('agent2-key.pem'),
      cert: fixtures.readKey('agent2-cert.pem'),
      sessionTimeout: 3600,
    });
    const agent = new https.Agent({
      keepAlive: false,
      keepAliveMsecs: 10_000,
      maxSockets: 5,
    });

    t.after(() => {
      agent.destroy();
      server.close();
    });

    const { promise, resolve } = createTestPromise();

    const expectedError = new Error();
    server.on('newSession', common.mustCall(() => {
      setImmediate(resolve);
      throw expectedError;
    }, 2));

    server.on('error', common.mustCall((e) => {
      assert.strictEqual(e, expectedError);
    }, 2));

    await new Promise((res) => server.listen(0, res));

    // We might need one or two requests to trigger the `resumeSession` event
    const clientOptions = {
      port: server.address().port,
      host: '127.0.0.1',
      rejectUnauthorized: false,
    };
    // First connection: Establish a session
    const socket = tls.connect(clientOptions, common.mustCall(() => {
    // Close immediately to allow session resumption on next connect
      socket.end();
    }));

    socket.on('error', common.mustNotCall());

    socket.on('close', common.mustCall());

    await promise;
  });

  it('OCSPRequest listener synchronous error emits tlsClientError', async (t) => {
    const server = tls.createServer({
      key: fixtures.readKey('agent2-key.pem'),
      cert: fixtures.readKey('agent2-cert.pem'),
      requestOCSP: true,
    });

    t.after(() => server.close());

    const { promise, resolve, reject } = createTestPromise();

    const expectedError = new Error();
    server.on('OCSPRequest', (cert, issuer, cb) => {
      throw expectedError;
    });

    server.on('tlsClientError', common.mustCall((err, socket) => {
      try {
        assert.strictEqual(err, expectedError);
        socket.destroy();
        resolve();
      } catch (e) {
        reject(e);
      }
    }));
    server.on('secureConnection', common.mustNotCall('secureConnection listener'));
    server.on('error', common.mustNotCall('server error listener'));

    await new Promise((res) => server.listen(0, res));

    const client = tls.connect({
      port: server.address().port,
      host: '127.0.0.1',
      rejectUnauthorized: false,
      requestOCSP: true,
    });

    client.on('error', common.mustCall());

    await promise;
  });

  it('resumeSession listener synchronous error emits tlsClientError', async (t) => {
    const server = tls.createServer({
      key: fixtures.readKey('agent2-key.pem'),
      cert: fixtures.readKey('agent2-cert.pem'),
      sessionTimeout: 3600,
    });
    const agent = new https.Agent({
      keepAlive: true,
      keepAliveMsecs: 10_000,
      maxSockets: 5,
    });

    t.after(() => {
      agent.destroy();
      server.close();
    });

    const { promise, resolve, reject } = createTestPromise();

    const expectedError = new Error();
    server.on('resumeSession', (id, cb) => {
      throw expectedError;
    });

    server.on('tlsClientError', common.mustCall((err, socket) => {
      try {
        assert.strictEqual(err, expectedError);
        socket.destroy();
        resolve();
      } catch (e) {
        reject(e);
      }
    }));
    server.on('error', common.mustNotCall('server error listener'));

    await new Promise((res) => server.listen(0, res));

    const expectErrorOnResume = common.expectsError();

    // We might need one or two requests to trigger the `resumeSession` event
    let triggeredOnFirstRequest = true;
    const reqOptions = {
      port: server.address().port,
      host: '127.0.0.1',
      path: '/',
      method: 'GET',
      agent,
      rejectUnauthorized: false,
    };
    const req = https.request(reqOptions, (res) => {
      triggeredOnFirstRequest = false;
      res.resume();
      res.on('end', () => {
        const req = https.request(reqOptions, (res) => {
          res.resume();
        });
        req.on('error', expectErrorOnResume);
        req.end();
      });
    });

    req.on('error', expectErrorOnResume);
    req.end();

    await promise;

    if (!triggeredOnFirstRequest) {
      // The second request would have received the error instead.
      req.off('error', expectErrorOnResume);
    }
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

    client.on('error', common.mustCall());

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
    server.on('secureConnection', common.mustNotCall('secureConnection listener'));

    await new Promise((res) => server.listen(0, res));

    const client = tls.connect({
      port: server.address().port,
      host: '127.0.0.1',
      rejectUnauthorized: false,
      ALPNProtocols: ['http/1.1', 'h2'],
    });

    client.on('error', common.mustCall());

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
    server.on('secureConnection', common.mustNotCall('secureConnection listener'));

    await new Promise((res) => server.listen(0, res));

    const client = tls.connect({
      port: server.address().port,
      host: '127.0.0.1',
      rejectUnauthorized: false,
      ALPNProtocols: ['http/1.1'],
    });

    client.on('error', common.mustCall());

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

    server.on('secureConnection', common.mustNotCall('secureConnection listener'));

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

    server.on('secureConnection', common.mustNotCall('secureConnection listener'));

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

  // Test 7: SNI callback throwing should emit tlsClientError
  it('SNICallback throwing emits tlsClientError', async (t) => {
    const server = tls.createServer({
      key: fixtures.readKey('agent2-key.pem'),
      cert: fixtures.readKey('agent2-cert.pem'),
      SNICallback: (servername, cb) => {
        throw new Error('Intentional SNI callback error');
      },
    });

    t.after(() => server.close());

    const { promise, resolve, reject } = createTestPromise();

    server.on('tlsClientError', common.mustCall((err, socket) => {
      try {
        assert.ok(err instanceof Error);
        assert.strictEqual(err.message, 'Intentional SNI callback error');
        socket.destroy();
        resolve();
      } catch (e) {
        reject(e);
      }
    }));
    server.on('secureConnection', common.mustNotCall('secureConnection listener'));

    await new Promise((res) => server.listen(0, res));

    const client = tls.connect({
      port: server.address().port,
      host: '127.0.0.1',
      servername: 'evil.attacker.com',
      rejectUnauthorized: false,
    });

    client.on('error', common.mustCall());

    await promise;
  });

  // Test 8: SNI callback with validation error should emit tlsClientError
  it('SNICallback validation error emits tlsClientError', async (t) => {
    const server = tls.createServer({
      key: fixtures.readKey('agent2-key.pem'),
      cert: fixtures.readKey('agent2-cert.pem'),
      SNICallback: (servername, cb) => {
        // Simulate common developer pattern: throw on unknown servername
        if (servername !== 'expected.example.com') {
          throw new Error(`Unknown servername: ${servername}`);
        }
        cb(null, null);
      },
    });

    t.after(() => server.close());

    const { promise, resolve, reject } = createTestPromise();

    server.on('tlsClientError', common.mustCall((err, socket) => {
      try {
        assert.ok(err instanceof Error);
        assert.ok(err.message.includes('Unknown servername'));
        socket.destroy();
        resolve();
      } catch (e) {
        reject(e);
      }
    }));
    server.on('secureConnection', common.mustNotCall('secureConnection listener'));

    await new Promise((res) => server.listen(0, res));

    const client = tls.connect({
      port: server.address().port,
      host: '127.0.0.1',
      servername: 'unexpected.domain.com',
      rejectUnauthorized: false,
    });

    client.on('error', common.mustCall());

    await promise;
  });
});
