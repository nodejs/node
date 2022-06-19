// Flags: --experimental-import-meta-resolve
import '../common/index.mjs';
import assert from 'assert';

const dirname = import.meta.url.slice(0, import.meta.url.lastIndexOf('/') + 1);
const fixtures = dirname.slice(0, dirname.lastIndexOf('/', dirname.length - 2) +
    1) + 'fixtures/';

assert.strictEqual(
  import.meta.resolve('./test-esm-import-meta.mjs'),
  dirname + 'test-esm-import-meta.mjs',
);

assert.throws(
  () => import.meta.resolve('./notfound.mjs'),
  { code: 'ERR_MODULE_NOT_FOUND' },
);

assert.strictEqual(
  import.meta.resolve('../fixtures/empty-with-bom.txt'),
  fixtures + 'empty-with-bom.txt',
);

assert.strictEqual(
  import.meta.resolve('../fixtures/'),
  fixtures,
);

assert.strictEqual(
  import.meta.resolve('../fixtures/', import.meta.url),
  fixtures,
);

assert.strictEqual(
  import.meta.resolve('../fixtures/', new URL(import.meta.url)),
  fixtures,
);

[[], {}, Symbol(), 0, 1, 1n, 1.1, () => {}, true, false].forEach((arg) => {
  assert.throws(
    () => import.meta.resolve('../fixtures/', arg),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

assert.strictEqual(
  import.meta.resolve('baz/', fixtures),
  fixtures + 'node_modules/baz/',
);
