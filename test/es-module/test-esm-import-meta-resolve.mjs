// Flags: --experimental-import-meta-resolve
import { mustCall } from '../common/index.mjs';
import assert from 'assert';

const dirname = import.meta.url.slice(0, import.meta.url.lastIndexOf('/') + 1);
const fixtures = dirname.slice(0, dirname.lastIndexOf('/', dirname.length - 2) +
    1) + 'fixtures/';

(async () => {
  assert.strictEqual(await import.meta.resolve('./test-esm-import-meta.mjs'),
                     dirname + 'test-esm-import-meta.mjs');
  try {
    await import.meta.resolve('./notfound.mjs');
    assert.fail();
  } catch (e) {
    assert.strictEqual(e.code, 'ERR_MODULE_NOT_FOUND');
  }
  assert.strictEqual(
    await import.meta.resolve('../fixtures/empty-with-bom.txt'),
    fixtures + 'empty-with-bom.txt');
  assert.strictEqual(await import.meta.resolve('../fixtures/'), fixtures);
  assert.strictEqual(
    await import.meta.resolve('../fixtures/', import.meta.url),
    fixtures);
  assert.strictEqual(
    await import.meta.resolve('../fixtures/', new URL(import.meta.url)),
    fixtures);
  await Promise.all(
    [[], {}, Symbol(), 0, 1, 1n, 1.1, () => {}, true, false].map((arg) =>
      assert.rejects(import.meta.resolve('../fixtures/', arg), {
        code: 'ERR_INVALID_ARG_TYPE',
      })
    )
  );
  assert.strictEqual(await import.meta.resolve('baz/', fixtures),
                     fixtures + 'node_modules/baz/');
})().then(mustCall());
