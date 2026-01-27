import '../common/index.mjs';
import assert from 'assert';
import fs from 'fs';

// Test importing a simple virtual ES module
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/hello.mjs', 'export const message = "hello from vfs";');
  myVfs.mount('/virtual');

  const { message } = await import('/virtual/hello.mjs');
  assert.strictEqual(message, 'hello from vfs');

  myVfs.unmount();
}

// Test importing a virtual module with default export
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/default.mjs', 'export default { name: "test", value: 42 };');
  myVfs.mount('/virtual2');

  const mod = await import('/virtual2/default.mjs');
  assert.strictEqual(mod.default.name, 'test');
  assert.strictEqual(mod.default.value, 42);

  myVfs.unmount();
}

// Test importing a virtual module that imports another virtual module
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/utils.mjs', 'export function add(a, b) { return a + b; }');
  myVfs.writeFileSync('/main.mjs', `
    import { add } from '/virtual3/utils.mjs';
    export const result = add(10, 20);
  `);
  myVfs.mount('/virtual3');

  const { result } = await import('/virtual3/main.mjs');
  assert.strictEqual(result, 30);

  myVfs.unmount();
}

// Test importing with relative paths
{
  const myVfs = fs.createVirtual();
  myVfs.mkdirSync('/lib', { recursive: true });
  myVfs.writeFileSync('/lib/helper.mjs', 'export const helper = () => "helped";');
  myVfs.writeFileSync('/lib/index.mjs', `
    import { helper } from './helper.mjs';
    export const output = helper();
  `);
  myVfs.mount('/virtual4');

  const { output } = await import('/virtual4/lib/index.mjs');
  assert.strictEqual(output, 'helped');

  myVfs.unmount();
}

// Test importing JSON from VFS (with import assertion)
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/data.json', JSON.stringify({ items: [1, 2, 3], enabled: true }));
  myVfs.mount('/virtual5');

  const data = await import('/virtual5/data.json', { with: { type: 'json' } });
  assert.deepStrictEqual(data.default.items, [1, 2, 3]);
  assert.strictEqual(data.default.enabled, true);

  myVfs.unmount();
}

// Test that real modules still work when VFS is mounted
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/test.mjs', 'export const x = 1;');
  myVfs.mount('/virtual6');

  // Import from node: should still work
  const assertMod = await import('node:assert');
  assert.strictEqual(typeof assertMod.strictEqual, 'function');

  myVfs.unmount();
}

// Test mixed CJS and ESM - ESM importing from VFS while CJS also works
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/esm-module.mjs', 'export const esmValue = "esm";');
  myVfs.writeFileSync('/cjs-module.js', 'module.exports = { cjsValue: "cjs" };');
  myVfs.mount('/virtual8');

  const { esmValue } = await import('/virtual8/esm-module.mjs');
  assert.strictEqual(esmValue, 'esm');

  // CJS require should also work (via createRequire)
  const { createRequire } = await import('module');
  const require = createRequire(import.meta.url);
  const { cjsValue } = require('/virtual8/cjs-module.js');
  assert.strictEqual(cjsValue, 'cjs');

  myVfs.unmount();
}
