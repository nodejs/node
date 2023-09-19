'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeySync,
  generateKeyPairSync,
  KeyObject,
} = require('crypto');
const { subtle } = globalThis.crypto;
const { createContext } = require('vm');
const {
  MessageChannel,
  Worker,
  moveMessagePortToContext,
  parentPort
} = require('worker_threads');

function keyToString(key) {
  let ret;
  if (key instanceof CryptoKey) {
    key = KeyObject.from(key);
  }
  if (key.type === 'secret') {
    ret = key.export().toString('hex');
  } else {
    ret = key.export({ type: 'pkcs1', format: 'pem' });
  }
  return ret;
}

// Worker threads simply reply with their representation of the received key.
if (process.env.HAS_STARTED_WORKER) {
  return parentPort.once('message', ({ key }) => {
    parentPort.postMessage(keyToString(key));
  });
}

// Don't use isMainThread to allow running this test inside a worker.
process.env.HAS_STARTED_WORKER = 1;

(async () => {
  // The main thread generates keys and passes them to worker threads.
  const secretKey = generateKeySync('aes', { length: 128 });
  const { publicKey, privateKey } = generateKeyPairSync('rsa', {
    modulusLength: 1024
  });
  const cryptoKey = await subtle.generateKey(
    { name: 'AES-CBC', length: 128 }, false, ['encrypt']);

  // Get immutable representations of all keys.
  const keys = [secretKey, publicKey, privateKey, cryptoKey]
             .map((key) => [key, keyToString(key)]);

  for (const [key, repr] of keys) {
    {
    // Test 1: No context change.
      const { port1, port2 } = new MessageChannel();

      port1.postMessage({ key });
      assert.strictEqual(keyToString(key), repr);

      port2.once('message', common.mustCall(({ key }) => {
        assert.strictEqual(keyToString(key), repr);
      }));
    }

    {
    // Test 2: Across threads.
      const worker = new Worker(__filename);
      worker.once('message', common.mustCall((receivedRepresentation) => {
        assert.strictEqual(receivedRepresentation, repr);
      }));
      worker.on('disconnect', () => console.log('disconnect'));
      worker.postMessage({ key });
    }

    {
    // Test 3: Across contexts (should not work).
      const { port1, port2 } = new MessageChannel();
      const context = createContext();
      const port2moved = moveMessagePortToContext(port2, context);
      assert(!(port2moved instanceof Object));

      // TODO(addaleax): Switch this to a 'messageerror' event once MessagePort
      // implements EventTarget fully and in a cross-context manner.
      port2moved.onmessageerror = common.mustCall((event) => {
        assert.strictEqual(event.data.code,
                           'ERR_MESSAGE_TARGET_CONTEXT_UNAVAILABLE');
      });

      port2moved.start();
      port1.postMessage({ key });
      port1.close();
    }
  }
})().then(common.mustCall());
