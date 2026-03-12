'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

// Test requiring a simple virtual module
// VFS internal path: /hello.js
// Mount point: /virtual
// External path: /virtual/hello.js
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/hello.js', 'module.exports = "hello from vfs";');
  myVfs.mount('/virtual');

  const result = require('/virtual/hello.js');
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
  myVfs.mount('/virtual2');

  const config = require('/virtual2/config.js');
  assert.strictEqual(config.name, 'test-config');
  assert.strictEqual(config.version, '1.0.0');
  assert.strictEqual(config.getValue(), 42);

  myVfs.unmount();
}

// Test requiring a virtual module that requires another virtual module
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/utils.js', `
    module.exports = {
      add: function(a, b) { return a + b; }
    };
  `);
  myVfs.writeFileSync('/main.js', `
    const utils = require('/virtual3/utils.js');
    module.exports = {
      sum: utils.add(10, 20)
    };
  `);
  myVfs.mount('/virtual3');

  const main = require('/virtual3/main.js');
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
  myVfs.mount('/virtual4');

  const data = require('/virtual4/data.json');
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
  myVfs.mount('/virtual5');

  const pkg = require('/virtual5/my-package');
  assert.strictEqual(pkg.loaded, true);

  myVfs.unmount();
}

// Test that real modules still work when VFS is mounted
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/test.js', 'module.exports = 1;');
  myVfs.mount('/virtual6');

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
  myVfs.mount('/virtual8');

  const result = require('/virtual8/lib/index.js');
  assert.strictEqual(result, 'helped');

  myVfs.unmount();
}

// Test fs.readFileSync interception when VFS is active
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'virtual content');
  myVfs.mount('/virtual9');

  const content = fs.readFileSync('/virtual9/file.txt', 'utf8');
  assert.strictEqual(content, 'virtual content');

  myVfs.unmount();
}

// Test requiring an ESM .mjs module from VFS
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/esm.mjs', 'export const msg = "hello from esm";');
  myVfs.mount('/virtual11');

  const mod = require('/virtual11/esm.mjs');
  assert.strictEqual(mod.msg, 'hello from esm');

  myVfs.unmount();
}

// Test requiring an ESM .mjs module with default export from VFS
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/esm-default.mjs', 'export default function() { return 42; }');
  myVfs.mount('/virtual12');

  const mod = require('/virtual12/esm-default.mjs');
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
  myVfs.mount('/virtual13');

  const mod = require('/virtual13/app/lib.js');
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
  myVfs.mount('/virtual14');

  const mod = require('/virtual14/project/src/utils/math.js');
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
  myVfs.mount('/virtual15');

  const mod = require('/virtual15/cjs-app/index.js');
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
  myVfs.mount('/virtual16');

  const mod = require('/virtual16/multi/src/main.js');
  assert.strictEqual(mod.result, 101);

  myVfs.unmount();
}

// Test require(esm): .mjs without any package.json loads as ESM
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/no-pkg.mjs',
                      'export const x = 1; export default "hello";');
  myVfs.mount('/virtual17');

  const mod = require('/virtual17/no-pkg.mjs');
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
  myVfs.mount('/virtual18');

  const mod = require('/virtual18/app/lib.mjs');
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
  myVfs.mount('/virtual19');

  const mod = require('/virtual19/cjs-pkg/esm.mjs');
  assert.strictEqual(mod.z, 99);

  myVfs.unmount();
}

// Test that unmounting stops interception
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/unmount-test.js', 'module.exports = "before unmount";');
  myVfs.mount('/virtual10');

  const result = require('/virtual10/unmount-test.js');
  assert.strictEqual(result, 'before unmount');

  myVfs.unmount();

  // After unmounting, the file should not be found
  assert.throws(() => {
    // Clear require cache first — the cache key is the platform-resolved path
    delete require.cache[path.resolve('/virtual10/unmount-test.js')];
    require('/virtual10/unmount-test.js');
  }, { code: 'MODULE_NOT_FOUND' });
}
