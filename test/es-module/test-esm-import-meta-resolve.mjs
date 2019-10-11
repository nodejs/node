// Flags: --experimental-import-meta-resolve
import '../common/index.mjs';
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
  assert.strictEqual(await import.meta.resolve('baz/', fixtures),
                     fixtures + 'node_modules/baz/');
})();
