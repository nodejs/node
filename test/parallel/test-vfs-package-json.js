'use strict';

// Flags: --expose-internals

require('../common');
const assert = require('assert');
const path = require('path');
const vfs = require('node:vfs');

// Test 1: read() reads package.json from VFS
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/pkg', { recursive: true });
  myVfs.writeFileSync('/pkg/package.json', JSON.stringify({
    name: 'test-pkg',
    type: 'module',
    main: './index.js',
    exports: { '.': './index.js' },
  }));
  myVfs.mount('/mnt/read-test');

  const packageJsonReader = require('internal/modules/package_json_reader');
  const result = packageJsonReader.read(
    path.resolve('/mnt/read-test/pkg/package.json'), {},
  );

  assert.strictEqual(result.exists, true);
  assert.strictEqual(result.name, 'test-pkg');
  assert.strictEqual(result.type, 'module');
  assert.strictEqual(result.main, './index.js');
  assert.deepStrictEqual(result.exports, { '.': './index.js' });
  assert.strictEqual(
    result.pjsonPath,
    path.resolve('/mnt/read-test/pkg/package.json'),
  );

  // Non-existent package.json returns exists: false
  const missing = packageJsonReader.read(
    path.resolve('/mnt/read-test/nope/package.json'), {},
  );
  assert.strictEqual(missing.exists, false);

  myVfs.unmount();
}

// Test 2: getNearestParentPackageJSON() walks up VFS directories
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/src/lib/deep', { recursive: true });
  myVfs.writeFileSync('/app/package.json', JSON.stringify({
    name: 'my-app',
    type: 'module',
  }));
  myVfs.writeFileSync('/app/src/lib/deep/module.js', '');
  myVfs.mount('/mnt/parent-test');

  const packageJsonReader = require('internal/modules/package_json_reader');
  const result = packageJsonReader.getNearestParentPackageJSON(
    path.resolve('/mnt/parent-test/app/src/lib/deep/module.js'),
  );

  assert.ok(result);
  assert.strictEqual(result.exists, true);
  assert.strictEqual(result.data.name, 'my-app');
  assert.strictEqual(result.data.type, 'module');
  assert.strictEqual(
    result.path,
    path.resolve('/mnt/parent-test/app/package.json'),
  );

  myVfs.unmount();
}

// Test 3: getPackageScopeConfig() returns correct scope from VFS
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/project/src', { recursive: true });
  myVfs.writeFileSync('/project/package.json', JSON.stringify({
    name: 'my-project',
    type: 'commonjs',
    exports: { '.': './main.js' },
  }));
  myVfs.writeFileSync('/project/src/index.js', '');
  myVfs.mount('/mnt/scope-test');

  const packageJsonReader = require('internal/modules/package_json_reader');
  const { pathToFileURL } = require('url');
  const scopeUrl = pathToFileURL(
    path.resolve('/mnt/scope-test/project/src/index.js'),
  ).href;
  const result = packageJsonReader.getPackageScopeConfig(scopeUrl);

  assert.strictEqual(result.exists, true);
  assert.strictEqual(result.type, 'commonjs');
  assert.strictEqual(result.name, 'my-project');
  assert.strictEqual(
    result.pjsonPath,
    path.resolve('/mnt/scope-test/project/package.json'),
  );

  // Path with no package.json returns exists: false
  const myVfs2 = vfs.create();
  myVfs2.mkdirSync('/empty/src', { recursive: true });
  myVfs2.writeFileSync('/empty/src/file.js', '');
  myVfs2.mount('/mnt/scope-empty');

  const emptyUrl = pathToFileURL(
    path.resolve('/mnt/scope-empty/empty/src/file.js'),
  ).href;
  const emptyResult = packageJsonReader.getPackageScopeConfig(emptyUrl);
  assert.strictEqual(emptyResult.exists, false);

  myVfs2.unmount();
  myVfs.unmount();
}

// Test 4: getPackageType() returns correct type from VFS
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/esm-app', { recursive: true });
  myVfs.writeFileSync('/esm-app/package.json', JSON.stringify({
    type: 'module',
  }));
  myVfs.writeFileSync('/esm-app/index.js', '');
  myVfs.mount('/mnt/type-test');

  const packageJsonReader = require('internal/modules/package_json_reader');
  const { pathToFileURL } = require('url');
  const typeUrl = pathToFileURL(
    path.resolve('/mnt/type-test/esm-app/index.js'),
  ).href;
  const type = packageJsonReader.getPackageType(typeUrl);
  assert.strictEqual(type, 'module');

  // No package.json => 'none'
  const myVfs2 = vfs.create();
  myVfs2.mkdirSync('/bare', { recursive: true });
  myVfs2.writeFileSync('/bare/file.js', '');
  myVfs2.mount('/mnt/type-empty');

  const noneUrl = pathToFileURL(
    path.resolve('/mnt/type-empty/bare/file.js'),
  ).href;
  const noneType = packageJsonReader.getPackageType(noneUrl);
  assert.strictEqual(noneType, 'none');

  myVfs2.unmount();
  myVfs.unmount();
}

// Test 5: End-to-end CJS require with package.json type detection
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/cjs-app', { recursive: true });
  myVfs.writeFileSync('/cjs-app/package.json', JSON.stringify({
    name: 'cjs-app',
    type: 'commonjs',
  }));
  myVfs.writeFileSync('/cjs-app/main.js',
                      'module.exports = { format: "cjs", ok: true };');
  myVfs.mount('/mnt/e2e-cjs');

  const result = require('/mnt/e2e-cjs/cjs-app/main.js');
  assert.strictEqual(result.format, 'cjs');
  assert.strictEqual(result.ok, true);

  myVfs.unmount();
}

// Test 6: End-to-end ESM import with VFS package type
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/esm', { recursive: true });
  myVfs.writeFileSync('/esm/package.json', JSON.stringify({
    type: 'module',
  }));
  myVfs.writeFileSync('/esm/mod.mjs', 'export const x = 42;');
  myVfs.mount('/mnt/e2e-esm');

  // Use .mjs to ensure ESM treatment regardless of package type
  const mod = require('/mnt/e2e-esm/esm/mod.mjs');
  assert.strictEqual(mod.x, 42);

  myVfs.unmount();
}
