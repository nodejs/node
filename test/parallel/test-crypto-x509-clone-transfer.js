// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { once } = require('node:events');
const {
  X509Certificate,
  createPrivateKey,
} = require('node:crypto');
const { readFileSync } = require('node:fs');
const {
  markAsUncloneable,
  MessageChannel,
  Worker,
} = require('node:worker_threads');
const fixtures = require('../common/fixtures');
const {
  isX509Certificate,
} = require('internal/crypto/x509');

const certData = readFileSync(fixtures.path('keys', 'agent1-cert.pem'));
const keyData = readFileSync(fixtures.path('keys', 'agent1-key.pem'));
const caData = readFileSync(fixtures.path('keys', 'ca1-cert.pem'));

const cert = new X509Certificate(certData);
const ca = new X509Certificate(caData);
const privateKey = createPrivateKey(keyData);

const dataCloneError = {
  code: 25,
  name: 'DataCloneError',
};

{
  const uncloneable = new X509Certificate(certData);
  markAsUncloneable(uncloneable);
  assert.throws(() => structuredClone(uncloneable), dataCloneError);

  const { port1, port2 } = new MessageChannel();
  assert.throws(() => port1.postMessage(uncloneable), dataCloneError);
  port1.close();
  port2.close();
}

function assertSameCertificate(original, clone) {
  assert.notStrictEqual(clone, original);
  assert.strictEqual(clone instanceof X509Certificate, true);
  assert.strictEqual(isX509Certificate(clone), true);
  assert.strictEqual(clone.subject, original.subject);
  assert.strictEqual(clone.issuer, original.issuer);
  assert.strictEqual(clone.fingerprint256, original.fingerprint256);
  assert.deepStrictEqual(clone.raw, original.raw);
  assert.deepStrictEqual(Reflect.ownKeys(clone), []);
  assert.strictEqual(clone.checkPrivateKey(privateKey), true);
  assert.strictEqual(clone.checkIssued(ca), true);
  assert.strictEqual(clone.verify(ca.publicKey), true);
}

async function roundTripViaMessageChannel(value) {
  const { port1, port2 } = new MessageChannel();
  port1.postMessage(value);
  const [received] = await once(port2, 'message');
  port1.close();
  port2.close();
  return received;
}

function workerMain() {
  const {
    X509Certificate: WorkerX509Certificate,
  } = require('node:crypto');
  const { parentPort } = require('node:worker_threads');

  parentPort.once('message', (cert) => {
    try {
      if (!(cert instanceof WorkerX509Certificate)) {
        throw new Error('X509Certificate brand was not preserved');
      }
      parentPort.postMessage({
        cert,
        ownKeyCount: Reflect.ownKeys(cert).length,
        subject: cert.subject,
      });
    } catch (error) {
      parentPort.postMessage({ error: error.stack || error.message });
    }
  });
}

async function roundTripViaWorker(value) {
  const worker = new Worker(
    `'use strict';(${workerMain.toString()})()`, { eval: true });

  worker.postMessage(value);
  const [message] = await once(worker, 'message');
  await worker.terminate();
  assert.strictEqual(message.error, undefined, message.error);
  assert.strictEqual(message.ownKeyCount, 0);
  assert.strictEqual(message.subject, value.subject);
  return message.cert;
}

(async () => {
  const cloned = structuredClone(cert);
  assertSameCertificate(cert, cloned);

  const viaPort = await roundTripViaMessageChannel(cert);
  assertSameCertificate(cert, viaPort);

  const clonedAgain = structuredClone(viaPort);
  assertSameCertificate(cert, clonedAgain);

  const viaWorker = await roundTripViaWorker(cert);
  assertSameCertificate(cert, viaWorker);
})().then(common.mustCall());
