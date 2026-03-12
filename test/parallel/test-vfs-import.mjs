import '../common/index.mjs';
import assert from 'assert';
import vfs from 'node:vfs';

// NOTE: Each test uses a unique mount path because ESM imports are cached
// by URL — unmounting does not clear the V8 module cache, so reusing a
// mount path would return stale cached modules from earlier tests.

// Test importing a simple virtual ES module
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/hello.mjs', 'export const message = "hello from vfs";');
  myVfs.mount('/esm-named');

  const { message } = await import('/esm-named/hello.mjs');
  assert.strictEqual(message, 'hello from vfs');

  myVfs.unmount();
}

// Test importing a virtual module with default export
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/default.mjs', 'export default { name: "test", value: 42 };');
  myVfs.mount('/esm-default');

  const mod = await import('/esm-default/default.mjs');
  assert.strictEqual(mod.default.name, 'test');
  assert.strictEqual(mod.default.value, 42);

  myVfs.unmount();
}

// Test importing a virtual module that imports another virtual module
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/utils.mjs', 'export function add(a, b) { return a + b; }');
  myVfs.writeFileSync('/main.mjs', `
    import { add } from '/esm-chain/utils.mjs';
    export const result = add(10, 20);
  `);
  myVfs.mount('/esm-chain');

  const { result } = await import('/esm-chain/main.mjs');
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
  myVfs.mount('/esm-relative');

  const { output } = await import('/esm-relative/lib/index.mjs');
  assert.strictEqual(output, 'helped');

  myVfs.unmount();
}

// Test importing JSON from VFS (with import assertion)
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/data.json', JSON.stringify({ items: [1, 2, 3], enabled: true }));
  myVfs.mount('/esm-json');

  const data = await import('/esm-json/data.json', { with: { type: 'json' } });
  assert.deepStrictEqual(data.default.items, [1, 2, 3]);
  assert.strictEqual(data.default.enabled, true);

  myVfs.unmount();
}

// Test that real modules still work when VFS is mounted
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/test.mjs', 'export const x = 1;');
  myVfs.mount('/esm-builtin');

  // Import from node: should still work
  const assertMod = await import('node:assert');
  assert.strictEqual(typeof assertMod.strictEqual, 'function');

  myVfs.unmount();
}

// Test mixed CJS and ESM - ESM importing from VFS while CJS also works
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/esm-module.mjs', 'export const esmValue = "esm";');
  myVfs.writeFileSync('/cjs-module.js', 'module.exports = { cjsValue: "cjs" };');
  myVfs.mount('/esm-mixed');

  const { esmValue } = await import('/esm-mixed/esm-module.mjs');
  assert.strictEqual(esmValue, 'esm');

  // CJS require should also work (via createRequire)
  const { createRequire } = await import('module');
  const require = createRequire(import.meta.url);
  const { cjsValue } = require('/esm-mixed/cjs-module.js');
  assert.strictEqual(cjsValue, 'cjs');

  myVfs.unmount();
}

// Test ESM bare specifier resolution from VFS node_modules.
// This sets up a proper node_modules structure inside VFS and imports
// using a bare specifier (e.g., import 'my-vfs-pkg') instead of an
// absolute path. This exercises the ESM default resolver's
// internalModuleStat and getPackageJSONURL code paths.
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
  // The importing module must also live inside the VFS mount so that
  // node_modules resolution walks upward from a VFS path.
  myVfs.writeFileSync(
    '/app/entry.mjs',
    "export { fromVfs } from 'my-vfs-pkg';",
  );
  myVfs.mount('/esm-bare');

  const { fromVfs } = await import('/esm-bare/app/entry.mjs');
  assert.strictEqual(fromVfs, true);

  myVfs.unmount();
}
