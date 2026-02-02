import '../common/index.mjs';
import assert from 'assert';
import path from 'path';
import { fileURLToPath, pathToFileURL } from 'url';

assert.strictEqual(Object.getPrototypeOf(import.meta), null);

const keys = ['dirname', 'filename', 'main', 'resolve', 'url'];
assert.deepStrictEqual(Reflect.ownKeys(import.meta), keys);

const descriptors = Object.getOwnPropertyDescriptors(import.meta);
for (const descriptor of Object.values(descriptors)) {
  delete descriptor.value; // Values are verified below.
  assert.deepStrictEqual(descriptor, {
    enumerable: true,
    writable: true,
    configurable: true,
  });
}

const filePath = fileURLToPath(import.meta.url);
assert.ok(path.isAbsolute(filePath));
assert.strictEqual(import.meta.url, pathToFileURL(filePath).href);

const suffix = path.join('test', 'es-module', 'test-esm-import-meta.mjs');
assert.ok(filePath.endsWith(suffix));
assert.strictEqual(import.meta.filename, filePath);
assert.strictEqual(import.meta.dirname, path.dirname(filePath));

// Verify that `data:` imports do not behave like `file:` imports.
import dataDirname from 'data:text/javascript,export default "dirname" in import.meta';
assert.strictEqual(dataDirname, false);
