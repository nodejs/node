// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('node:url');
const vfs = require('node:vfs');

const vfsImport = (path) => pathToFileURL(path).href;

// Test requiring a simple virtual module
// VFS internal path: /hello.js
// Logical prefix: /virtual — the actual mount point is returned by mount()
// External path: `${mountPoint}/hello.js`
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/hello.js', 'module.exports = "hello from vfs";');
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/hello.js`);
  assert.strictEqual(result, 'hello from vfs');

  myVfs.unmount();
}

// Test requiring a virtual module that exports an object
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/config.js', `
    module.exports = {
      name: 'test-config',
      version: '1.0.0',
      getValue: function() { return 42; }
    };
  `);
  const mountPoint = myVfs.mount();

  const config = require(`${mountPoint}/config.js`);
  assert.strictEqual(config.name, 'test-config');
  assert.strictEqual(config.version, '1.0.0');
  assert.strictEqual(config.getValue(), 42);

  myVfs.unmount();
}

// Test requiring a virtual module that requires another virtual module.
// Mount first so the embedded absolute specifier can use the mount point.
{
  const myVfs = vfs.create();
  const mountPoint = myVfs.mount();
  myVfs.writeFileSync(`${mountPoint}/utils.js`, `
    module.exports = {
      add: function(a, b) { return a + b; }
    };
  `);
  myVfs.writeFileSync(`${mountPoint}/main.js`, `
    const utils = require(${JSON.stringify(`${mountPoint}/utils.js`)});
    module.exports = {
      sum: utils.add(10, 20)
    };
  `);

  const main = require(`${mountPoint}/main.js`);
  assert.strictEqual(main.sum, 30);

  myVfs.unmount();
}

// Test requiring a JSON file from VFS
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/data.json', JSON.stringify({
    items: [1, 2, 3],
    enabled: true,
  }));
  const mountPoint = myVfs.mount();

  const data = require(`${mountPoint}/data.json`);
  assert.deepStrictEqual(data.items, [1, 2, 3]);
  assert.strictEqual(data.enabled, true);

  myVfs.unmount();
}

// Test virtual package.json resolution
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/my-package', { recursive: true });
  myVfs.writeFileSync('/my-package/package.json', JSON.stringify({
    name: 'my-package',
    main: 'index.js',
  }));
  myVfs.writeFileSync('/my-package/index.js', `
    module.exports = { loaded: true };
  `);
  const mountPoint = myVfs.mount();

  const pkg = require(`${mountPoint}/my-package`);
  assert.strictEqual(pkg.loaded, true);

  myVfs.unmount();
}

// Test that real modules still work when VFS is mounted
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/test.js', 'module.exports = 1;');
  myVfs.mount();

  // require('assert') should still work (builtin)
  assert.strictEqual(typeof assert.strictEqual, 'function');

  // Real file requires should still work
  const commonMod = require('../common');
  assert.ok(commonMod);

  myVfs.unmount();
}

// Test require with relative paths inside VFS module
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/lib', { recursive: true });
  myVfs.writeFileSync('/lib/helper.js', `
    module.exports = { help: function() { return 'helped'; } };
  `);
  myVfs.writeFileSync('/lib/index.js', `
    const helper = require('./helper.js');
    module.exports = helper.help();
  `);
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/lib/index.js`);
  assert.strictEqual(result, 'helped');

  myVfs.unmount();
}

// Test fs.readFileSync interception when VFS is active
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'virtual content');
  const mountPoint = myVfs.mount();

  const content = fs.readFileSync(`${mountPoint}/file.txt`, 'utf8');
  assert.strictEqual(content, 'virtual content');

  myVfs.unmount();
}

// Test requiring an ESM .mjs module from VFS
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/esm.mjs', 'export const msg = "hello from esm";');
  const mountPoint = myVfs.mount();

  const mod = require(`${mountPoint}/esm.mjs`);
  assert.strictEqual(mod.msg, 'hello from esm');

  myVfs.unmount();
}

// Test requiring an ESM .mjs module with default export from VFS
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/esm-default.mjs', 'export default function() { return 42; }');
  const mountPoint = myVfs.mount();

  const mod = require(`${mountPoint}/esm-default.mjs`);
  assert.strictEqual(mod.default(), 42);

  myVfs.unmount();
}

// Test require(esm): .js file detected as ESM via VFS package.json type:module
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app', { recursive: true });
  myVfs.writeFileSync('/app/package.json', JSON.stringify({
    name: 'esm-app',
    type: 'module',
  }));
  myVfs.writeFileSync('/app/lib.js',
                      'export const value = 42;' +
                      ' export function hello() { return "hi"; }');
  const mountPoint = myVfs.mount();

  const mod = require(`${mountPoint}/app/lib.js`);
  assert.strictEqual(mod.value, 42);
  assert.strictEqual(mod.hello(), 'hi');

  myVfs.unmount();
}

// Test require(esm): nested .js walks up to parent package.json in VFS
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/project/src/utils', { recursive: true });
  myVfs.writeFileSync('/project/package.json', JSON.stringify({
    name: 'nested-esm',
    type: 'module',
  }));
  myVfs.writeFileSync('/project/src/utils/math.js',
                      'export const add = (a, b) => a + b;' +
                      ' export default 99;');
  const mountPoint = myVfs.mount();

  const mod = require(`${mountPoint}/project/src/utils/math.js`);
  assert.strictEqual(mod.add(3, 4), 7);
  assert.strictEqual(mod.default, 99);

  myVfs.unmount();
}

// Test require(esm): .js without type field stays CJS
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/cjs-app', { recursive: true });
  myVfs.writeFileSync('/cjs-app/package.json', JSON.stringify({
    name: 'cjs-app',
  }));
  myVfs.writeFileSync('/cjs-app/index.js',
                      'module.exports = { cjs: true };');
  const mountPoint = myVfs.mount();

  const mod = require(`${mountPoint}/cjs-app/index.js`);
  assert.strictEqual(mod.cjs, true);

  myVfs.unmount();
}

// Test require(esm): ESM .js that imports another ESM .js in VFS
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/multi/src', { recursive: true });
  myVfs.writeFileSync('/multi/package.json', JSON.stringify({
    type: 'module',
  }));
  myVfs.writeFileSync('/multi/src/dep.js', 'export const X = 100;');
  myVfs.writeFileSync('/multi/src/main.js',
                      'import { X } from "./dep.js";' +
                      ' export const result = X + 1;');
  const mountPoint = myVfs.mount();

  const mod = require(`${mountPoint}/multi/src/main.js`);
  assert.strictEqual(mod.result, 101);

  myVfs.unmount();
}

// Test require(esm): .mjs without any package.json loads as ESM
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/no-pkg.mjs',
                      'export const x = 1; export default "hello";');
  const mountPoint = myVfs.mount();

  const mod = require(`${mountPoint}/no-pkg.mjs`);
  assert.strictEqual(mod.x, 1);
  assert.strictEqual(mod.default, 'hello');

  myVfs.unmount();
}

// Test require(esm): .mjs with package.json that has no type field
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app', { recursive: true });
  myVfs.writeFileSync('/app/package.json',
                      JSON.stringify({ name: 'no-type' }));
  myVfs.writeFileSync('/app/lib.mjs', 'export const val = 42;');
  const mountPoint = myVfs.mount();

  const mod = require(`${mountPoint}/app/lib.mjs`);
  assert.strictEqual(mod.val, 42);

  myVfs.unmount();
}

// Test require(esm): .mjs in type:commonjs package still loads as ESM
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/cjs-pkg', { recursive: true });
  myVfs.writeFileSync('/cjs-pkg/package.json', JSON.stringify({
    name: 'cjs-pkg',
    type: 'commonjs',
  }));
  myVfs.writeFileSync('/cjs-pkg/esm.mjs', 'export const z = 99;');
  const mountPoint = myVfs.mount();

  const mod = require(`${mountPoint}/cjs-pkg/esm.mjs`);
  assert.strictEqual(mod.z, 99);

  myVfs.unmount();
}

// Test CJS: package with "main" field resolves through VFS
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/pkg/lib', { recursive: true });
  myVfs.writeFileSync('/pkg/package.json', JSON.stringify({
    name: 'legacy-main-pkg',
    main: './lib/entry',
  }));
  myVfs.writeFileSync('/pkg/lib/entry.js', 'module.exports = "legacy-main";');
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/pkg`);
  assert.strictEqual(result, 'legacy-main');

  myVfs.unmount();
}

// Test CJS: package with no "main" field resolves index.js
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/pkg2', { recursive: true });
  myVfs.writeFileSync('/pkg2/package.json', JSON.stringify({
    name: 'no-main-pkg',
  }));
  myVfs.writeFileSync('/pkg2/index.js', 'module.exports = "index-fallback";');
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/pkg2`);
  assert.strictEqual(result, 'index-fallback');

  myVfs.unmount();
}

// Test ESM legacyMainResolve: import() a VFS package with "main" (no "exports")
// This triggers the ESM legacyMainResolve path in resolve.js via bare specifier
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/esm-legacy-main/lib', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/esm-legacy-main/package.json', JSON.stringify({
    name: 'esm-legacy-main',
    type: 'module',
    main: './lib/entry.js',
  }));
  myVfs.writeFileSync('/app/node_modules/esm-legacy-main/lib/entry.js',
                      'export const value = "esm-legacy-main";');
  myVfs.writeFileSync('/app/main.mjs',
                      'export { value } from "esm-legacy-main";');
  const mountPoint = myVfs.mount();

  import(vfsImport(`${mountPoint}/app/main.mjs`)).then(common.mustCall((mod) => {
    assert.strictEqual(mod.value, 'esm-legacy-main');
    myVfs.unmount();
  }));
}

// Test ESM legacyMainResolve: import() a VFS package with no "main" (index.js fallback)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app2/node_modules/esm-nomain', { recursive: true });
  myVfs.writeFileSync('/app2/node_modules/esm-nomain/package.json', JSON.stringify({
    name: 'esm-nomain',
    type: 'module',
  }));
  myVfs.writeFileSync('/app2/node_modules/esm-nomain/index.js',
                      'export const value = "esm-index-fallback";');
  myVfs.writeFileSync('/app2/main.mjs',
                      'export { value } from "esm-nomain";');
  const mountPoint = myVfs.mount();

  import(vfsImport(`${mountPoint}/app2/main.mjs`)).then(common.mustCall((mod) => {
    assert.strictEqual(mod.value, 'esm-index-fallback');
    myVfs.unmount();
  }));
}

// Test getFormatOfExtensionlessFile: extensionless JS file in type:module package
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/esm-pkg', { recursive: true });
  myVfs.writeFileSync('/esm-pkg/package.json', JSON.stringify({
    name: 'esm-pkg',
    type: 'module',
  }));
  myVfs.writeFileSync('/esm-pkg/entry', 'export const x = 123;');
  const mountPoint = myVfs.mount();

  // Use import() to trigger ESM loader path for extensionless file detection
  import(vfsImport(`${mountPoint}/esm-pkg/entry`)).then(common.mustCall((mod) => {
    assert.strictEqual(mod.x, 123);
    myVfs.unmount();
  }));
}

// Test that unmounting stops interception
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/unmount-test.js', 'module.exports = "before unmount";');
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/unmount-test.js`);
  assert.strictEqual(result, 'before unmount');

  myVfs.unmount();

  // After unmounting, the VFS hooks should not serve the file. On Windows,
  // the native loader may surface the NUL-backed former mount path as an
  // invalid package config while probing for package.json.
  assert.throws(() => {
    // Clear require cache first — the cache key is the resolved mounted path
    delete require.cache[path.join(mountPoint, 'unmount-test.js')];
    require(`${mountPoint}/unmount-test.js`);
  }, (err) => {
    if (common.isWindows) {
      assert.ok(
        ['ERR_INVALID_PACKAGE_CONFIG', 'MODULE_NOT_FOUND'].includes(err.code),
      );
    } else {
      assert.strictEqual(err.code, 'MODULE_NOT_FOUND');
    }
    return true;
  });
}
