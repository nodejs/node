import '../common/index.mjs';
import assert from 'assert';

assert.strictEqual(Object.getPrototypeOf(import.meta), null);

const keys = ['url'];
assert.deepStrictEqual(Reflect.ownKeys(import.meta), keys);

const descriptors = Object.getOwnPropertyDescriptors(import.meta);
for (const descriptor of Object.values(descriptors)) {
  delete descriptor.value; // Values are verified below.
  assert.deepStrictEqual(descriptor, {
    enumerable: true,
    writable: true,
    configurable: true
  });
}

const urlReg = /^file:\/\/\/.*\/test\/es-module\/test-esm-import-meta\.mjs$/;
assert(import.meta.url.match(urlReg));
