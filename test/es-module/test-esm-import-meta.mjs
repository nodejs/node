import '../common/index.mjs';
import assert from 'assert';

assert.strictEqual(Object.getPrototypeOf(import.meta), null);

const keys = ['dirname', 'filename', 'resolve', 'url'];
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

const dirReg = /^\/.*\/test\/es-module$/;
assert.match(import.meta.dirname, dirReg);

const fileReg = /^\/.*\/test\/es-module\/test-esm-import-meta\.mjs$/;
assert.match(import.meta.filename, fileReg);

// Verify that `data:` imports do not behave like `file:` imports.
import dataDirname from 'data:text/javascript,export default import.meta.dirname';
assert.strictEqual(dataDirname, undefined);
