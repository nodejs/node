// Flags: --experimental-dtls --no-warnings

// Test: dtls.createSecureContext() and the secureContext option.
//
// listen() and connect() used to build a context per call, with no way to
// share one. A context holds a parsed certificate, key and CA store, so a
// client opening many connections paid for a duplicate of all of it every
// time. This exposes the context and accepts it on both calls.

import { hasCrypto, mustNotCall, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const dtls = await import('node:dtls');
const { connect, createSecureContext, listen, DTLSSecureContext } = dtls;

const cert = fixtures.readKey('agent1-cert.pem').toString();
const key = fixtures.readKey('agent1-key.pem').toString();
const ca = fixtures.readKey('ca1-cert.pem').toString();

// The context reports which side it was built for.
{
  const server = createSecureContext({ cert, key, isServer: true });
  const client = createSecureContext({ ca: [ca] });

  assert.ok(server instanceof DTLSSecureContext);
  assert.strictEqual(server.isServer, true);
  assert.strictEqual(client.isServer, false);
}

// It cannot be constructed directly.
assert.throws(() => new DTLSSecureContext(),
              { code: 'ERR_ILLEGAL_CONSTRUCTOR' });

// One server context serves several endpoints, and one client context
// serves connections to all of them.
{
  const serverContext = createSecureContext({ cert, key, isServer: true });
  const clientContext = createSecureContext({ ca: [ca] });

  const endpoints = [];
  for (let i = 0; i < 3; i++) {
    endpoints.push(listen((session) => {
      session.onmessage = (data) => session.send(data);
    }, { secureContext: serverContext, host: '127.0.0.1', port: 0 }));
  }

  for (const endpoint of endpoints) {
    const client = connect('127.0.0.1', endpoint.address.port, {
      secureContext: clientContext,
      servername: 'agent1',
    });

    await client.opened;
    assert.strictEqual(client.authorized, true);

    const echoed = Promise.withResolvers();
    client.onmessage = (data) => echoed.resolve(data.toString());
    client.send('shared');
    assert.strictEqual(await echoed.promise, 'shared');

    await client.close();
  }

  for (const endpoint of endpoints) {
    await endpoint.close();
  }
}

// The verified identity stays per connection. It is bound to the session
// with SSL_set1_host(), not to the context, so one context can be used
// against different peer names -- and still rejects the wrong one.
{
  const serverContext = createSecureContext({ cert, key, isServer: true });
  const clientContext = createSecureContext({ ca: [ca] });

  const endpoint = listen(() => {}, {
    secureContext: serverContext, host: '127.0.0.1', port: 0,
  });

  const good = connect('127.0.0.1', endpoint.address.port, {
    secureContext: clientContext, servername: 'agent1',
  });
  await good.opened;
  assert.strictEqual(good.authorized, true);
  await good.close();

  // Same context, a name the certificate does not cover.
  const bad = connect('127.0.0.1', endpoint.address.port, {
    secureContext: clientContext, servername: 'not-agent1',
  });
  await assert.rejects(bad.opened, { name: 'Error' });

  await endpoint.close();
}

// listen() does not demand cert and key when the context already has them.
{
  const serverContext = createSecureContext({ cert, key, isServer: true });
  const endpoint = listen(mustNotCall(), {
    secureContext: serverContext, host: '127.0.0.1', port: 0,
  });
  assert.ok(endpoint.address.port > 0);
  await endpoint.close();
}

// A context belongs to one side and cannot be handed to the other: isServer
// picks the OpenSSL method when the context is built.
{
  const serverContext = createSecureContext({ cert, key, isServer: true });
  const clientContext = createSecureContext({ ca: [ca] });

  const endpoint = listen(mustNotCall(), {
    cert, key, host: '127.0.0.1', port: 0,
  });

  assert.throws(() => connect('127.0.0.1', endpoint.address.port, {
    secureContext: serverContext,
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  assert.throws(() => listen(mustNotCall(), {
    secureContext: clientContext, host: '127.0.0.1', port: 0,
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  await endpoint.close();
}

// Options that belong to the context are refused alongside one, rather than
// being silently ignored.
{
  const serverContext = createSecureContext({ cert, key, isServer: true });
  const clientContext = createSecureContext({ ca: [ca] });

  for (const [name, value] of [
    ['cert', cert],
    ['key', key],
    ['ca', [ca]],
    ['ciphers', 'ALL'],
    ['requestCert', true],
    ['sessionIdContext', 'x'],
  ]) {
    assert.throws(() => listen(mustNotCall(), {
      secureContext: serverContext,
      [name]: value,
      host: '127.0.0.1',
      port: 0,
    }), { code: 'ERR_INVALID_ARG_VALUE' }, `listen() accepted ${name}`);
  }

  // This one does accept a connection below, so it gets a real handler.
  const endpoint = listen(() => {}, {
    cert, key, host: '127.0.0.1', port: 0,
  });

  assert.throws(() => connect('127.0.0.1', endpoint.address.port, {
    secureContext: clientContext, rejectUnauthorized: false,
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  // Per-connection options remain legal.
  const client = connect('127.0.0.1', endpoint.address.port, {
    secureContext: clientContext, servername: 'agent1',
  });
  await client.opened;
  await client.close();

  await endpoint.close();
}

// Something that is not a context is rejected by type.
for (const value of [{}, null, 'ctx', 42]) {
  assert.throws(() => listen(mustNotCall(), {
    secureContext: value, cert, key, host: '127.0.0.1', port: 0,
  }), { code: 'ERR_INVALID_ARG_TYPE' });
}
