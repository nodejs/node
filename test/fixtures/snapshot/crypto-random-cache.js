'use strict';

const assert = require('node:assert');
const {
  addDeserializeCallback,
  addSerializeCallback,
  setDeserializeMainFunction,
} = require('node:v8').startupSnapshot;

// Register before loading crypto to ensure its cleanup takes precedence even
// over user callbacks that were registered earlier.
let deserialized;
addDeserializeCallback(() => {
  deserialized = sample();
});

const {
  randomInt,
  randomUUID,
  randomUUIDv7,
} = require('node:crypto');

const buildValues = [];
const max = 2 ** 48 - 1;

function primeCaches() {
  buildValues.push(
    randomUUID(),
    randomInt(max),
    randomUUID({ disableEntropyCache: true }),
  );
}

switch (process.env.NODE_TEST_CRYPTO_RANDOM_MODE) {
  case 'uuid':
    primeCaches();
    break;
  case 'uuidv7':
    buildValues.push(
      randomUUIDv7(),
      randomInt(max),
      randomUUIDv7({ disableEntropyCache: true }),
    );
    break;
  case 'object-url':
    URL.revokeObjectURL(URL.createObjectURL(new Blob(['snapshot'])));
    break;
  case 'async':
    randomInt(max, (err, value) => {
      assert.ifError(err);
      buildValues.push(value);
    });
    break;
  case 'serialize':
    primeCaches();
    // A user callback can refill the caches after an earlier cleanup callback.
    addSerializeCallback(primeCaches);
    break;
  case 'microtask':
    primeCaches();
    addSerializeCallback(() => queueMicrotask(primeCaches));
    break;
  case 'next-tick':
    primeCaches();
    addSerializeCallback(() => process.nextTick(primeCaches));
    break;
  case 'exit':
    primeCaches();
    process.once('exit', primeCaches);
    break;
  case 'unprimed':
    break;
  default:
    assert.fail('Unknown cache priming mode');
}

function sample() {
  return {
    uuid: Array.from({ length: 4 }, () => randomUUID()),
    // Exclude the timestamp and version so clock changes cannot hide replay.
    uuidv7: Array.from({ length: 4 }, () => randomUUIDv7().slice(15)),
    webcrypto: Array.from({ length: 4 }, () => globalThis.crypto.randomUUID()),
    int: Array.from({ length: 4 }, () => randomInt(max)),
  };
}

setDeserializeMainFunction(async () => {
  const main = sample();
  main.asyncInt = await Promise.all(Array.from({ length: 4 }, () => new Promise((resolve, reject) => {
    randomInt(max, (err, value) => {
      if (err) reject(err);
      else resolve(value);
    });
  })));
  console.log(JSON.stringify({ buildValues, deserialized, main }));
});
