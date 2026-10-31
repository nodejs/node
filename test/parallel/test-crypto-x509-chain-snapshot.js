'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { X509Certificate } = require('node:crypto');
const { once } = require('node:events');
const tls = require('node:tls');
const { deserialize, serialize } = require('node:v8');
const {
  MessageChannel,
  Worker,
} = require('node:worker_threads');
const fixtures = require('../common/fixtures');

const leaf = new X509Certificate(fixtures.readKey('agent6-cert.pem'));
const issuer = new X509Certificate(fixtures.readKey('ca3-cert.pem'));

function assertChain(actual) {
  assert(actual instanceof X509Certificate);
  assert.deepStrictEqual(actual.raw, leaf.raw);
  assert.strictEqual(actual.issuerCertificate, actual.issuerCertificate);
  assert(actual.issuerCertificate instanceof X509Certificate);
  assert.deepStrictEqual(actual.issuerCertificate.raw, issuer.raw);
  assert.strictEqual(actual.issuerCertificate.issuerCertificate, undefined);
}

function legacyChainRaw(certificate) {
  const result = [];
  const seen = new Set();
  while (certificate !== undefined && !seen.has(certificate)) {
    seen.add(certificate);
    result.push(certificate.raw);
    certificate = certificate.issuerCertificate;
  }
  return result;
}

async function cloneViaMessagePort(certificate) {
  const { port1, port2 } = new MessageChannel();
  port1.postMessage(certificate);
  const [clone] = await once(port2, 'message');
  port1.close();
  port2.close();
  return clone;
}

async function cloneViaWorker(certificate) {
  const worker = new Worker(`
    'use strict';
    const { parentPort } = require('node:worker_threads');
    parentPort.once('message', (certificate) => {
      parentPort.postMessage(certificate);
    });
  `, { eval: true });
  worker.postMessage(certificate);
  const [clone] = await once(worker, 'message');
  await worker.terminate();
  return clone;
}

const server = tls.createServer({
  key: fixtures.readKey('agent6-key.pem'),
  cert: fixtures.readKey('agent6-cert.pem'),
}, (socket) => socket.end());

(async () => {
  server.listen(0);
  await once(server, 'listening');

  const socket = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
  });
  await once(socket, 'secureConnect');

  const first = socket.getPeerX509Certificate();
  assertChain(first);

  const firstLegacy = socket.getPeerCertificate(true);
  const second = socket.getPeerX509Certificate();
  const secondLegacy = socket.getPeerCertificate(true);
  assertChain(second);
  assert.deepStrictEqual(legacyChainRaw(firstLegacy), [leaf.raw, issuer.raw]);
  assert.deepStrictEqual(legacyChainRaw(secondLegacy), [leaf.raw, issuer.raw]);

  assertChain(structuredClone(first));
  assertChain(await cloneViaMessagePort(first));
  assertChain(await cloneViaWorker(first));
  assertChain(deserialize(serialize(first)));

  socket.destroy();
  server.close();
  await once(server, 'close');
})().then(common.mustCall());
