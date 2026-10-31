'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { X509Certificate } = require('node:crypto');
const { once } = require('node:events');
const tls = require('node:tls');
const { deserialize, serialize } = require('node:v8');
const { MessageChannel, Worker } = require('node:worker_threads');
const fixtures = require('../common/fixtures');

const leaf = new X509Certificate(fixtures.readKey('agent6-cert.pem'));
const issuer = new X509Certificate(fixtures.readKey('ca3-cert.pem'));
const unrelated = new X509Certificate(fixtures.readKey('agent1-cert.pem'));
const key = fixtures.readKey('agent6-key.pem');
const chains = [
  [leaf, issuer, unrelated],
  [leaf, unrelated, issuer],
  [leaf, issuer, issuer],
  [leaf, issuer, unrelated, issuer],
  [leaf, leaf, issuer],
];

function assertChain(actual, expected) {
  const seen = new Set();
  for (const certificate of expected) {
    assert(actual instanceof X509Certificate);
    assert(!seen.has(actual));
    seen.add(actual);
    assert.deepStrictEqual(actual.raw, certificate.raw);
    assert.strictEqual(actual.issuerCertificate, actual.issuerCertificate);
    actual = actual.issuerCertificate;
  }
  assert.strictEqual(actual, undefined);
}

async function assertClones(certificate, chain) {
  assertChain(structuredClone(certificate), chain);
  assertChain(deserialize(serialize(certificate)), chain);

  const { port1, port2 } = new MessageChannel();
  port1.postMessage(certificate);
  const [messageClone] = await once(port2, 'message');
  port1.close();
  port2.close();
  assertChain(messageClone, chain);

  const worker = new Worker(`
    const { parentPort } = require('node:worker_threads');
    parentPort.once('message', (certificate) => {
      parentPort.postMessage(certificate);
    });
  `, { eval: true });
  worker.postMessage(certificate);
  const [workerClone] = await once(worker, 'message');
  await worker.terminate();
  assertChain(workerClone, chain);
}

async function checkPeer(socket, chain) {
  const first = socket.getPeerX509Certificate();
  assertChain(first, chain);
  for (let i = 0; i < 2; i++) {
    assert.deepStrictEqual(socket.getPeerCertificate(true).raw, leaf.raw);
    assertChain(socket.getPeerX509Certificate(), chain);
  }
  await assertClones(first, chain);
}

async function test(chain) {
  // Encode each certificate separately: agent6-cert.pem contains its issuer.
  const cert = chain.map((certificate) => certificate.toString()).join('');
  const server = tls.createServer({
    key,
    cert,
    requestCert: true,
    rejectUnauthorized: false,
  });
  server.listen(0);
  await once(server, 'listening');

  const peer = once(server, 'secureConnection');
  const client = tls.connect({
    port: server.address().port,
    key,
    cert,
    rejectUnauthorized: false,
  });
  await once(client, 'secureConnect');
  const [socket] = await peer;

  // Client and server certificate stacks handle the leaf differently; both
  // sockets must expose every certificate sent by the peer exactly once.
  await checkPeer(client, chain);
  await checkPeer(socket, chain);

  // Clones must remain usable after their TLS session has been destroyed.
  const clone = structuredClone(client.getPeerX509Certificate());
  client.destroy();
  socket.destroy();
  const closed = once(server, 'close');
  server.close();
  await closed;
  assertChain(clone, chain);
}

(async () => {
  for (const chain of chains) await test(chain);
})().then(common.mustCall());
