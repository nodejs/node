// Flags: --experimental-vfs
import '../common/index.mjs';
import assert from 'assert';
import { pathToFileURL } from 'node:url';
import vfs from 'node:vfs';

const vfsImport = (path) => pathToFileURL(path).href;

// ESM imports are cached by URL and unmounting does not clear the V8 module
// cache; each vfs.create() gets its own layer id so mount points are unique.

// Test importing a simple virtual ES module
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/hello.mjs', 'export const message = "hello from vfs";');
  const mountPoint = myVfs.mount();

  const { message } = await import(vfsImport(`${mountPoint}/hello.mjs`));
  assert.strictEqual(message, 'hello from vfs');

  myVfs.unmount();
}

// Test importing a virtual module with default export
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/default.mjs', 'export default { name: "test", value: 42 };');
  const mountPoint = myVfs.mount();

  const mod = await import(vfsImport(`${mountPoint}/default.mjs`));
  assert.strictEqual(mod.default.name, 'test');
  assert.strictEqual(mod.default.value, 42);

  myVfs.unmount();
}

// Test importing a virtual module that imports another virtual module.
// Mount first so the embedded absolute specifier can use the mount point.
{
  const myVfs = vfs.create();
  const mountPoint = myVfs.mount();
  myVfs.writeFileSync(`${mountPoint}/utils.mjs`,
                      'export function add(a, b) { return a + b; }');
  myVfs.writeFileSync(`${mountPoint}/main.mjs`, `
    import { add } from ${JSON.stringify(vfsImport(`${mountPoint}/utils.mjs`))};
    export const result = add(10, 20);
  `);

  const { result } = await import(vfsImport(`${mountPoint}/main.mjs`));
  assert.strictEqual(result, 30);

  myVfs.unmount();
}

// Test importing with relative paths
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/lib', { recursive: true });
  myVfs.writeFileSync('/lib/helper.mjs', 'export const helper = () => "helped";');
  myVfs.writeFileSync('/lib/index.mjs', `
    import { helper } from './helper.mjs';
    export const output = helper();
  `);
  const mountPoint = myVfs.mount();

  const { output } = await import(vfsImport(`${mountPoint}/lib/index.mjs`));
  assert.strictEqual(output, 'helped');

  myVfs.unmount();
}

// Test importing JSON from VFS (with import assertion)
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/data.json', JSON.stringify({ items: [1, 2, 3], enabled: true }));
  const mountPoint = myVfs.mount();

  const data = await import(vfsImport(`${mountPoint}/data.json`), { with: { type: 'json' } });
  assert.deepStrictEqual(data.default.items, [1, 2, 3]);
  assert.strictEqual(data.default.enabled, true);

  myVfs.unmount();
}

// Test that real modules still work when VFS is mounted
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/test.mjs', 'export const x = 1;');
  myVfs.mount();

  const assertMod = await import('node:assert');
  assert.strictEqual(typeof assertMod.strictEqual, 'function');

  myVfs.unmount();
}

// Test mixed CJS and ESM - ESM importing from VFS while CJS also works
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/esm-module.mjs', 'export const esmValue = "esm";');
  myVfs.writeFileSync('/cjs-module.js', 'module.exports = { cjsValue: "cjs" };');
  const mountPoint = myVfs.mount();

  const { esmValue } = await import(vfsImport(`${mountPoint}/esm-module.mjs`));
  assert.strictEqual(esmValue, 'esm');

  const { createRequire } = await import('module');
  const require = createRequire(import.meta.url);
  const { cjsValue } = require(`${mountPoint}/cjs-module.js`);
  assert.strictEqual(cjsValue, 'cjs');

  myVfs.unmount();
}

// Test ESM bare specifier resolution from VFS node_modules.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/my-vfs-pkg', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/my-vfs-pkg/package.json', JSON.stringify({
    name: 'my-vfs-pkg',
    type: 'module',
    exports: { '.': './index.mjs' },
  }));
  myVfs.writeFileSync(
    '/app/node_modules/my-vfs-pkg/index.mjs',
    'export const fromVfs = true;',
  );
  // Importing module must live in the VFS mount so node_modules walk stays inside VFS.
  myVfs.writeFileSync(
    '/app/entry.mjs',
    "export { fromVfs } from 'my-vfs-pkg';",
  );
  const mountPoint = myVfs.mount();

  const { fromVfs } = await import(vfsImport(`${mountPoint}/app/entry.mjs`));
  assert.strictEqual(fromVfs, true);

  myVfs.unmount();
}
