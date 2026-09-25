// Flags: --experimental-vfs
import '../common/index.mjs';
import assert from 'assert';
import { createRequire } from 'module';
import { pathToFileURL } from 'node:url';
import vfs from 'node:vfs';

const require = createRequire(import.meta.url);
const vfsImport = (path) => pathToFileURL(path).href;

// mount() returns the actual mount point (reserved under os.devNull);
// each test captures it and uses it for requires/imports.

// Test: CJS bare specifier resolution with exports string shorthand
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/str-pkg', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/str-pkg/package.json', JSON.stringify({
    name: 'str-pkg',
    exports: './main.js',
  }));
  myVfs.writeFileSync(
    '/app/node_modules/str-pkg/main.js',
    'module.exports = { strExport: true };',
  );
  myVfs.writeFileSync(
    '/app/entry.js',
    "module.exports = require('str-pkg');",
  );
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/app/entry.js`);
  assert.strictEqual(result.strExport, true);

  myVfs.unmount();
}

// Test: Conditional exports with import/require/default conditions
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/cond-pkg', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/cond-pkg/package.json', JSON.stringify({
    name: 'cond-pkg',
    exports: {
      import: './esm.mjs',
      require: './cjs.js',
      default: './default.js',
    },
  }));
  myVfs.writeFileSync(
    '/app/node_modules/cond-pkg/esm.mjs',
    'export const source = "esm";',
  );
  myVfs.writeFileSync(
    '/app/node_modules/cond-pkg/cjs.js',
    'module.exports = { source: "cjs" };',
  );
  myVfs.writeFileSync(
    '/app/node_modules/cond-pkg/default.js',
    'module.exports = { source: "default" };',
  );
  myVfs.writeFileSync(
    '/app/esm-entry.mjs',
    "export { source } from 'cond-pkg';",
  );
  myVfs.writeFileSync(
    '/app/cjs-entry.js',
    "module.exports = require('cond-pkg');",
  );
  const mountPoint = myVfs.mount();

  const esmResult = await import(vfsImport(`${mountPoint}/app/esm-entry.mjs`));
  assert.strictEqual(esmResult.source, 'esm');

  const cjsResult = require(`${mountPoint}/app/cjs-entry.js`);
  assert.strictEqual(cjsResult.source, 'cjs');

  myVfs.unmount();
}

// Test: Subpath exports map
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/sub-pkg/lib', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/sub-pkg/package.json', JSON.stringify({
    name: 'sub-pkg',
    exports: {
      '.': './lib/index.mjs',
      './feature': './lib/feature.mjs',
    },
  }));
  myVfs.writeFileSync(
    '/app/node_modules/sub-pkg/lib/index.mjs',
    'export const main = true;',
  );
  myVfs.writeFileSync(
    '/app/node_modules/sub-pkg/lib/feature.mjs',
    'export const feature = true;',
  );
  myVfs.writeFileSync(
    '/app/entry.mjs',
    `
    import { main } from 'sub-pkg';
    import { feature } from 'sub-pkg/feature';
    export { main, feature };
    `,
  );
  const mountPoint = myVfs.mount();

  const result = await import(vfsImport(`${mountPoint}/app/entry.mjs`));
  assert.strictEqual(result.main, true);
  assert.strictEqual(result.feature, true);

  myVfs.unmount();
}

// Test: Subpath exports with conditional object target
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/sub-cond-pkg', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/sub-cond-pkg/package.json', JSON.stringify({
    name: 'sub-cond-pkg',
    exports: {
      '.': {
        import: './esm.mjs',
        require: './cjs.js',
      },
    },
  }));
  myVfs.writeFileSync(
    '/app/node_modules/sub-cond-pkg/esm.mjs',
    'export const fromSubCond = "esm";',
  );
  myVfs.writeFileSync(
    '/app/node_modules/sub-cond-pkg/cjs.js',
    'module.exports = { fromSubCond: "cjs" };',
  );
  myVfs.writeFileSync(
    '/app/entry2.mjs',
    "export { fromSubCond } from 'sub-cond-pkg';",
  );
  const mountPoint = myVfs.mount();

  const result = await import(vfsImport(`${mountPoint}/app/entry2.mjs`));
  assert.strictEqual(result.fromSubCond, 'esm');

  myVfs.unmount();
}

// Test: Nested conditional exports (e.g. node -> import/require)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/nested-cond-pkg', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/nested-cond-pkg/package.json', JSON.stringify({
    name: 'nested-cond-pkg',
    exports: {
      node: {
        import: './node-esm.mjs',
        require: './node-cjs.js',
      },
      default: './fallback.js',
    },
  }));
  myVfs.writeFileSync(
    '/app/node_modules/nested-cond-pkg/node-esm.mjs',
    'export const nested = "node-esm";',
  );
  myVfs.writeFileSync(
    '/app/node_modules/nested-cond-pkg/node-cjs.js',
    'module.exports = { nested: "node-cjs" };',
  );
  myVfs.writeFileSync(
    '/app/node_modules/nested-cond-pkg/fallback.js',
    'module.exports = { nested: "fallback" };',
  );
  myVfs.writeFileSync(
    '/app/entry3.mjs',
    "export { nested } from 'nested-cond-pkg';",
  );
  const mountPoint = myVfs.mount();

  const result = await import(vfsImport(`${mountPoint}/app/entry3.mjs`));
  assert.strictEqual(result.nested, 'node-esm');

  myVfs.unmount();
}

// Test: Package without exports, using main field
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/main-pkg', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/main-pkg/package.json', JSON.stringify({
    name: 'main-pkg',
    main: './lib/entry.js',
  }));
  myVfs.mkdirSync('/app/node_modules/main-pkg/lib', { recursive: true });
  myVfs.writeFileSync(
    '/app/node_modules/main-pkg/lib/entry.js',
    'module.exports = { fromMain: true };',
  );
  myVfs.writeFileSync(
    '/app/entry4.js',
    "module.exports = require('main-pkg');",
  );
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/app/entry4.js`);
  assert.strictEqual(result.fromMain, true);

  myVfs.unmount();
}

// Test: Package without exports/main, fallback to index.js
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/index-pkg', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/index-pkg/package.json', JSON.stringify({
    name: 'index-pkg',
  }));
  myVfs.writeFileSync(
    '/app/node_modules/index-pkg/index.js',
    'module.exports = { fromIndex: true };',
  );
  myVfs.writeFileSync(
    '/app/entry5.js',
    "module.exports = require('index-pkg');",
  );
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/app/entry5.js`);
  assert.strictEqual(result.fromIndex, true);

  myVfs.unmount();
}

// Test: Extension resolution (require without file extension)
{
  const myVfs = vfs.create();
  // The embedded specifier must be an absolute mounted path, so mount first.
  const mountPoint = myVfs.mount();
  myVfs.mkdirSync(`${mountPoint}/lib`, { recursive: true });
  myVfs.writeFileSync(`${mountPoint}/lib/utils.js`, 'module.exports = { ext: "js" };');
  myVfs.writeFileSync(
    `${mountPoint}/lib/main.js`,
    `module.exports = require(${JSON.stringify(`${mountPoint}/lib/utils`)});`,
  );

  const result = require(`${mountPoint}/lib/main.js`);
  assert.strictEqual(result.ext, 'js');

  myVfs.unmount();
}

// Test: Extension resolution with .json
{
  const myVfs = vfs.create();
  const mountPoint = myVfs.mount();
  myVfs.writeFileSync(`${mountPoint}/data.json`, JSON.stringify({ ext: 'json' }));
  myVfs.writeFileSync(
    `${mountPoint}/reader.js`,
    `module.exports = require(${JSON.stringify(`${mountPoint}/data`)});`,
  );

  const result = require(`${mountPoint}/reader.js`);
  assert.strictEqual(result.ext, 'json');

  myVfs.unmount();
}

// Test: Scoped package resolution (@scope/pkg)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/@myorg/mylib', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/@myorg/mylib/package.json', JSON.stringify({
    name: '@myorg/mylib',
    type: 'module',
    exports: './index.mjs',
  }));
  myVfs.writeFileSync(
    '/app/node_modules/@myorg/mylib/index.mjs',
    'export const scoped = true;',
  );
  myVfs.writeFileSync(
    '/app/scoped-entry.mjs',
    "export { scoped } from '@myorg/mylib';",
  );
  const mountPoint = myVfs.mount();

  const result = await import(vfsImport(`${mountPoint}/app/scoped-entry.mjs`));
  assert.strictEqual(result.scoped, true);

  myVfs.unmount();
}

// Test: Scoped package with subpath
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/@myorg/utils/lib', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/@myorg/utils/package.json', JSON.stringify({
    name: '@myorg/utils',
    exports: {
      '.': './lib/index.mjs',
      './helpers': './lib/helpers.mjs',
    },
  }));
  myVfs.writeFileSync(
    '/app/node_modules/@myorg/utils/lib/index.mjs',
    'export const main = true;',
  );
  myVfs.writeFileSync(
    '/app/node_modules/@myorg/utils/lib/helpers.mjs',
    'export const helpers = true;',
  );
  myVfs.writeFileSync(
    '/app/scoped-sub-entry.mjs',
    `
    import { helpers } from '@myorg/utils/helpers';
    export { helpers };
    `,
  );
  const mountPoint = myVfs.mount();

  const result = await import(vfsImport(`${mountPoint}/app/scoped-sub-entry.mjs`));
  assert.strictEqual(result.helpers, true);

  myVfs.unmount();
}

// Test: .js file with type: module in package.json -> ESM format
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/package.json', JSON.stringify({ type: 'module' }));
  myVfs.writeFileSync('/mod.js', 'export const fromModule = true;');
  const mountPoint = myVfs.mount();

  const result = await import(vfsImport(`${mountPoint}/mod.js`));
  assert.strictEqual(result.fromModule, true);

  myVfs.unmount();
}

// Test: .cjs always treated as CommonJS regardless of package type
{
  const myVfs = vfs.create();
  const mountPoint = myVfs.mount();
  myVfs.writeFileSync(`${mountPoint}/package.json`, JSON.stringify({ type: 'module' }));
  myVfs.writeFileSync(`${mountPoint}/helper.cjs`, 'module.exports = { cjsAlways: true };');
  myVfs.writeFileSync(
    `${mountPoint}/use-cjs.js`,
    `
    import { createRequire } from 'module';
    const require = createRequire(import.meta.url);
    const result = require(${JSON.stringify(`${mountPoint}/helper.cjs`)});
    export const cjsAlways = result.cjsAlways;
    `,
  );

  const result = await import(vfsImport(`${mountPoint}/use-cjs.js`));
  assert.strictEqual(result.cjsAlways, true);

  myVfs.unmount();
}

// Test: file: URL specifier
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/fileurl.mjs', 'export const fromFileUrl = true;');
  const mountPoint = myVfs.mount();

  const result = await import(vfsImport(`${mountPoint}/fileurl.mjs`));
  assert.strictEqual(result.fromFileUrl, true);

  myVfs.unmount();
}

// Test: Package with main field requiring extension resolution
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir-pkg', { recursive: true });
  myVfs.writeFileSync('/dir-pkg/package.json', JSON.stringify({
    name: 'dir-pkg',
    main: './entry',
  }));
  myVfs.writeFileSync(
    '/dir-pkg/entry.js',
    'module.exports = { dirPkg: true };',
  );
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/dir-pkg`);
  assert.strictEqual(result.dirPkg, true);

  myVfs.unmount();
}

// Test: Bare specifier with package subpath (no exports, direct file)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/direct-pkg/lib', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/direct-pkg/package.json', JSON.stringify({
    name: 'direct-pkg',
  }));
  myVfs.writeFileSync(
    '/app/node_modules/direct-pkg/lib/util.js',
    'module.exports = { directSub: true };',
  );
  myVfs.writeFileSync(
    '/app/entry-sub.js',
    "module.exports = require('direct-pkg/lib/util.js');",
  );
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/app/entry-sub.js`);
  assert.strictEqual(result.directSub, true);

  myVfs.unmount();
}

// Test: Bare specifier subpath with extension resolution
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/ext-pkg/lib', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/ext-pkg/package.json', JSON.stringify({
    name: 'ext-pkg',
  }));
  myVfs.writeFileSync(
    '/app/node_modules/ext-pkg/lib/util.js',
    'module.exports = { extSub: true };',
  );
  myVfs.writeFileSync(
    '/app/entry-ext.js',
    "module.exports = require('ext-pkg/lib/util');",
  );
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/app/entry-ext.js`);
  assert.strictEqual(result.extSub, true);

  myVfs.unmount();
}

// Test: Bare specifier main field with extension resolution
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/main-ext-pkg', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/main-ext-pkg/package.json', JSON.stringify({
    name: 'main-ext-pkg',
    main: './entry',
  }));
  myVfs.writeFileSync(
    '/app/node_modules/main-ext-pkg/entry.js',
    'module.exports = { mainExt: true };',
  );
  myVfs.writeFileSync(
    '/app/entry-main-ext.js',
    "module.exports = require('main-ext-pkg');",
  );
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/app/entry-main-ext.js`);
  assert.strictEqual(result.mainExt, true);

  myVfs.unmount();
}

// Test: exports with array value (fallback array)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/arr-exp-pkg', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/arr-exp-pkg/package.json', JSON.stringify({
    name: 'arr-exp-pkg',
    exports: {
      '.': ['./index.js', './fallback.js'],
    },
    main: './index.js',
  }));
  myVfs.writeFileSync(
    '/app/node_modules/arr-exp-pkg/index.js',
    'module.exports = { arrExport: true };',
  );
  myVfs.writeFileSync(
    '/app/entry-arr.js',
    "module.exports = require('arr-exp-pkg');",
  );
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/app/entry-arr.js`);
  assert.strictEqual(result.arrExport, true);

  myVfs.unmount();
}

// Test: Exports with "default" condition
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/default-pkg', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/default-pkg/package.json', JSON.stringify({
    name: 'default-pkg',
    exports: {
      '.': {
        browser: './browser.js',
        default: './default.mjs',
      },
    },
  }));
  myVfs.writeFileSync(
    '/app/node_modules/default-pkg/default.mjs',
    'export const fromDefault = true;',
  );
  myVfs.writeFileSync(
    '/app/entry-default.mjs',
    "export { fromDefault } from 'default-pkg';",
  );
  const mountPoint = myVfs.mount();

  const result = await import(vfsImport(`${mountPoint}/app/entry-default.mjs`));
  assert.strictEqual(result.fromDefault, true);

  myVfs.unmount();
}

// Test: Package.json type "commonjs" explicitly set for .js
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/package.json', JSON.stringify({ type: 'commonjs' }));
  myVfs.writeFileSync('/explicit-cjs.js', 'module.exports = { explicitCjs: true };');
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/explicit-cjs.js`);
  assert.strictEqual(result.explicitCjs, true);

  myVfs.unmount();
}

// Test: .js file with no package.json -> defaults to commonjs
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/no-pkg.js', 'module.exports = { noPkg: true };');
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/no-pkg.js`);
  assert.strictEqual(result.noPkg, true);

  myVfs.unmount();
}

// Test: Package.json type walk stops at node_modules boundary
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/package.json', JSON.stringify({ type: 'module' }));
  myVfs.mkdirSync('/node_modules/inner', { recursive: true });
  myVfs.writeFileSync(
    '/node_modules/inner/index.js',
    'module.exports = { inner: true };',
  );
  const mountPoint = myVfs.mount();

  // Walk stops at node_modules; type:module from root is not inherited.
  const result = require(`${mountPoint}/node_modules/inner/index.js`);
  assert.strictEqual(result.inner, true);

  myVfs.unmount();
}

// Test: Invalid package.json in directory resolution
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/bad-json-dir', { recursive: true });
  myVfs.writeFileSync('/bad-json-dir/package.json', '{ invalid json }');
  myVfs.writeFileSync(
    '/bad-json-dir/index.js',
    'module.exports = { fallbackIndex: true };',
  );
  const mountPoint = myVfs.mount();

  // Malformed package.json raises ERR_INVALID_PACKAGE_CONFIG after nodejs/node#48606.
  assert.throws(() => require(`${mountPoint}/bad-json-dir`),
                { code: 'ERR_INVALID_PACKAGE_CONFIG' });

  myVfs.unmount();
}

// Test: Invalid package.json in type walk-up
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/package.json', '{ broken json }');
  myVfs.writeFileSync('/no-type.js', 'module.exports = { noType: true };');
  const mountPoint = myVfs.mount();

  assert.throws(() => require(`${mountPoint}/no-type.js`),
                { code: 'ERR_INVALID_PACKAGE_CONFIG' });

  myVfs.unmount();
}

// Test: Scoped package without slash (just @scope/name)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/node_modules/@solo/pkg', { recursive: true });
  myVfs.writeFileSync('/app/node_modules/@solo/pkg/package.json', JSON.stringify({
    name: '@solo/pkg',
    main: './index.js',
  }));
  myVfs.writeFileSync(
    '/app/node_modules/@solo/pkg/index.js',
    'module.exports = { solo: true };',
  );
  myVfs.writeFileSync(
    '/app/entry-solo.js',
    "module.exports = require('@solo/pkg');",
  );
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/app/entry-solo.js`);
  assert.strictEqual(result.solo, true);

  myVfs.unmount();
}

// Test: node: builtin passthrough
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/use-builtin.mjs', `
    import path from 'node:path';
    export const sep = path.sep;
  `);
  const mountPoint = myVfs.mount();

  const result = await import(vfsImport(`${mountPoint}/use-builtin.mjs`));
  assert.strictEqual(typeof result.sep, 'string');

  myVfs.unmount();
}

// Test: JSON import with type assertion
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/data.json', JSON.stringify({ preformat: true }));
  const mountPoint = myVfs.mount();

  const result = await import(vfsImport(`${mountPoint}/data.json`), { with: { type: 'json' } });
  assert.strictEqual(result.default.preformat, true);

  myVfs.unmount();
}

// Test: File with unknown extension -> defaults to commonjs
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/data.txt', 'module.exports = { txt: true };');
  const mountPoint = myVfs.mount();

  const result = require(`${mountPoint}/data.txt`);
  assert.strictEqual(result.txt, true);

  myVfs.unmount();
}
