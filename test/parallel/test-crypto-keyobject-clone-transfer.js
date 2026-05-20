'use strict';

// KeyObject instances must survive structured cloning with their
// native backing data and hidden JS slot semantics preserved.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { once } = require('node:events');
const {
  createHmac,
  createSecretKey,
  generateKeyPairSync,
  sign,
  verify,
} = require('node:crypto');
const { MessageChannel, Worker } = require('node:worker_threads');
const { types: { isKeyObject } } = require('node:util');

function assertNoOwnKeys(key) {
  assert.deepStrictEqual(Object.getOwnPropertySymbols(key), []);
  assert.deepStrictEqual(Object.getOwnPropertyNames(key), []);
  assert.deepStrictEqual(Reflect.ownKeys(key), []);
}

function assertSameKeyObject(original, clone) {
  assert.notStrictEqual(original, clone);
  assert.strictEqual(isKeyObject(clone), true);
  assert.strictEqual(original.type, clone.type);
  assert.strictEqual(original.equals(clone), true);
  assert.deepStrictEqual(original, clone);
  if (clone.type === 'secret') {
    assert.strictEqual(original.symmetricKeySize, clone.symmetricKeySize);
  } else {
    assert.strictEqual(original.asymmetricKeyType, clone.asymmetricKeyType);
    assert.deepStrictEqual(
      original.asymmetricKeyDetails,
      clone.asymmetricKeyDetails);
  }
  assertNoOwnKeys(original);
  assertNoOwnKeys(clone);
}

async function roundTripViaMessageChannel(key) {
  const { port1, port2 } = new MessageChannel();
  port1.postMessage(key);
  const [received] = await once(port2, 'message');
  port1.close();
  port2.close();
  return received;
}

async function roundTripViaWorker(key) {
  const worker = new Worker(`
    'use strict';
    const { parentPort } = require('node:worker_threads');
    const { types: { isKeyObject } } = require('node:util');

    parentPort.once('message', ({ key, expectedType }) => {
      try {
        if (!isKeyObject(key) || key.type !== expectedType) {
          throw new Error('KeyObject slot mismatch in worker');
        }
        parentPort.postMessage({ key });
      } catch (err) {
        parentPort.postMessage({ error: err.stack || err.message });
      }
    });
  `, { eval: true });

  worker.postMessage({ key, expectedType: key.type });
  const [msg] = await once(worker, 'message');
  await worker.terminate();

  assert.strictEqual(msg.error, undefined, msg.error);
  return msg.key;
}

function hmacDigest(key) {
  return createHmac('sha256', key).update('payload').digest('hex');
}

(async () => {
  const secret = createSecretKey(Buffer.alloc(16));
  const { publicKey, privateKey } = generateKeyPairSync('rsa', {
    modulusLength: 1024,
  });

  for (const key of [secret, publicKey, privateKey]) {
    const cloned = structuredClone(key);
    assertSameKeyObject(key, cloned);

    const viaPort = await roundTripViaMessageChannel(key);
    assertSameKeyObject(key, viaPort);

    const clonedAgain = structuredClone(viaPort);
    assertSameKeyObject(key, clonedAgain);

    const viaWorker = await roundTripViaWorker(key);
    assertSameKeyObject(key, viaWorker);
  }

  const secretClones = [
    secret,
    structuredClone(secret),
    await roundTripViaMessageChannel(secret),
    await roundTripViaWorker(secret),
  ];
  const digest = hmacDigest(secret);
  for (const key of secretClones) {
    assert.strictEqual(hmacDigest(key), digest);
  }

  const data = Buffer.from('payload');
  const publicClones = [
    publicKey,
    structuredClone(publicKey),
    await roundTripViaMessageChannel(publicKey),
    await roundTripViaWorker(publicKey),
  ];
  const privateClones = [
    privateKey,
    structuredClone(privateKey),
    await roundTripViaMessageChannel(privateKey),
    await roundTripViaWorker(privateKey),
  ];

  const signature = sign('sha256', data, privateKey);
  for (const key of publicClones) {
    assert.strictEqual(verify('sha256', data, key, signature), true);
  }
  for (const key of privateClones) {
    const clonedSignature = sign('sha256', data, key);
    assert.strictEqual(verify('sha256', data, publicKey, clonedSignature), true);
  }
})().then(common.mustCall());
