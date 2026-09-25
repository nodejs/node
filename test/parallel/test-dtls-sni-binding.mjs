// Flags: --experimental-dtls --no-warnings --expose-gc --expose-internals

// Test: invariants of the SNI binding that the public API no longer reaches.
//
// SNI is configured when a context is created and cannot be changed
// afterwards, so neither of the situations below can be produced through
// dtls.createSecureContext() or dtls.listen(). Both were reachable when `sni`
// was a listen() option applied to whatever context it was given, and both
// were bugs then. They are tested against the binding directly, because the
// binding is what has to stay correct if the option ever moves back.

import { hasCrypto, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { createSecureContext } = await import('node:dtls');
const { kHandle } = (await import('internal/dtls/symbols')).default;

const key = (name) => fixtures.readKey(name).toString();
const cert = key('agent1-cert.pem');
const privateKey = key('agent1-key.pem');

const serverContext = () =>
  createSecureContext({ cert, key: privateKey, isServer: true });

// The binding holds SNI contexts weakly, so a context naming itself does not
// keep itself alive.
//
// BaseObjectPtr is a reference count rather than an edge the collector can
// see, so a self-reference never reached zero: the context, its SSL_CTX, its
// key and its certificate stayed for the life of the process. Enough of them
// trips the base_object_count_ assertion in ~Realm() at exit, so this test
// fails by aborting, not by asserting, if the holding goes back to strong.
{
  const registry = new FinalizationRegistry(() => { finalized++; });
  let finalized = 0;
  const total = 20;

  for (let i = 0; i < total; i++) {
    // A context serving one of its own names, and a pair naming each other.
    const self = serverContext();
    const first = serverContext();
    const second = serverContext();
    registry.register(self, 'self');
    registry.register(first, 'first');
    registry.register(second, 'second');

    self[kHandle].setSNIContexts(['self.example', self[kHandle]], undefined);
    first[kHandle].setSNIContexts(['second.example', second[kHandle]],
                                  undefined);
    second[kHandle].setSNIContexts(['first.example', first[kHandle]],
                                   undefined);
  }

  for (let i = 0; i < 8 && finalized < total * 3; i++) {
    globalThis.gc();
    await new Promise((resolve) => setImmediate(resolve));
  }

  // Not all of them: whether the most recent is still reachable from a local
  // is not something a test can pin down. Most is the signal, and holding
  // them strongly collects none of the self-referencing ones at all.
  assert.ok(finalized > total,
            `only ${finalized}/${total * 3} contexts were collected`);
}

// Configuring SNI without a callback clears any callback already installed.
//
// Only ever assigning it meant a context given a map after a callback kept
// the callback, which for SNI is a fail-open: a map with no '*' entry means
// only the names in it are served, and a leftover callback answers the rest.
{
  const context = serverContext();
  const identity = serverContext();

  let calls = 0;
  context[kHandle].setSNIContexts([], () => { calls++; return identity; });
  context[kHandle].setSNIContexts(['known.example', identity[kHandle]],
                                  undefined);

  const { connect, listen } = await import('node:dtls');
  const server = listen(() => {}, {
    secureContext: context, host: '127.0.0.1', port: 0,
  });

  const unmatched = connect('127.0.0.1', server.address.port, {
    servername: 'unknown.example',
    rejectUnauthorized: false,
  });
  await assert.rejects(unmatched.opened, { name: 'Error' });
  assert.strictEqual(calls, 0);

  const matched = connect('127.0.0.1', server.address.port, {
    servername: 'known.example',
    rejectUnauthorized: false,
  });
  await matched.opened;
  await matched.close();

  await server.close();
}
