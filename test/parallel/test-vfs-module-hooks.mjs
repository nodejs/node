import '../common/index.mjs';
import assert from 'assert';
import { createRequire } from 'module';
import vfs from 'node:vfs';

const require = createRequire(import.meta.url);

// NOTE: Each test uses a different mount path (/mh1, /mh2, etc.)
// because ESM imports are cached by URL.

// =================================================================
// Test: CJS bare specifier resolution with exports string shorthand
// =================================================================
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
  myVfs.mount('/mh1');

  const result = require('/mh1/app/entry.js');
  assert.strictEqual(result.strExport, true);

  myVfs.unmount();
}

// =================================================================
// Test: Conditional exports with import/require/default conditions
// =================================================================
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
  // ESM entry that imports via bare specifier
  myVfs.writeFileSync(
    '/app/esm-entry.mjs',
    "export { source } from 'cond-pkg';",
  );
  // CJS entry that requires via bare specifier
  myVfs.writeFileSync(
    '/app/cjs-entry.js',
    "module.exports = require('cond-pkg');",
  );
  myVfs.mount('/mh2');

  // ESM import should get the 'import' condition
  const esmResult = await import('/mh2/app/esm-entry.mjs');
  assert.strictEqual(esmResult.source, 'esm');

  // CJS require should get the 'require' condition
  const cjsResult = require('/mh2/app/cjs-entry.js');
  assert.strictEqual(cjsResult.source, 'cjs');

  myVfs.unmount();
}

// =================================================================
// Test: Subpath exports map
// =================================================================
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
  myVfs.mount('/mh3');

  const result = await import('/mh3/app/entry.mjs');
  assert.strictEqual(result.main, true);
  assert.strictEqual(result.feature, true);

  myVfs.unmount();
}

// =================================================================
// Test: Subpath exports with conditional object target
// =================================================================
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
  myVfs.mount('/mh4');

  const result = await import('/mh4/app/entry2.mjs');
  assert.strictEqual(result.fromSubCond, 'esm');

  myVfs.unmount();
}

// =================================================================
// Test: Nested conditional exports (e.g. node → import/require)
// =================================================================
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
  myVfs.mount('/mh5');

  const result = await import('/mh5/app/entry3.mjs');
  assert.strictEqual(result.nested, 'node-esm');

  myVfs.unmount();
}

// =================================================================
// Test: Package without exports, using main field
// =================================================================
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
  myVfs.mount('/mh6');

  const result = require('/mh6/app/entry4.js');
  assert.strictEqual(result.fromMain, true);

  myVfs.unmount();
}

// =================================================================
// Test: Package without exports/main, fallback to index.js
// =================================================================
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
  myVfs.mount('/mh7');

  const result = require('/mh7/app/entry5.js');
  assert.strictEqual(result.fromIndex, true);

  myVfs.unmount();
}

// =================================================================
// Test: Extension resolution (require without file extension)
// =================================================================
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/lib', { recursive: true });
  // File without .js extension in specifier
  myVfs.writeFileSync('/lib/utils.js', 'module.exports = { ext: "js" };');
  myVfs.writeFileSync(
    '/lib/main.js',
    "module.exports = require('/mh8/lib/utils');",
  );
  myVfs.mount('/mh8');

  const result = require('/mh8/lib/main.js');
  assert.strictEqual(result.ext, 'js');

  myVfs.unmount();
}

// =================================================================
// Test: Extension resolution with .json
// =================================================================
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/data.json', JSON.stringify({ ext: 'json' }));
  myVfs.writeFileSync(
    '/reader.js',
    "module.exports = require('/mh9/data');",
  );
  myVfs.mount('/mh9');

  const result = require('/mh9/reader.js');
  assert.strictEqual(result.ext, 'json');

  myVfs.unmount();
}

// =================================================================
// Test: Scoped package resolution (@scope/pkg)
// =================================================================
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
  myVfs.mount('/mh11');

  const result = await import('/mh11/app/scoped-entry.mjs');
  assert.strictEqual(result.scoped, true);

  myVfs.unmount();
}

// =================================================================
// Test: Scoped package with subpath
// =================================================================
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
  myVfs.mount('/mh12');

  const result = await import('/mh12/app/scoped-sub-entry.mjs');
  assert.strictEqual(result.helpers, true);

  myVfs.unmount();
}

// =================================================================
// Test: .js file with type: module in package.json → ESM format
// =================================================================
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/package.json', JSON.stringify({ type: 'module' }));
  myVfs.writeFileSync('/mod.js', 'export const fromModule = true;');
  myVfs.mount('/mh13');

  const result = await import('/mh13/mod.js');
  assert.strictEqual(result.fromModule, true);

  myVfs.unmount();
}

// =================================================================
// Test: .cjs always treated as CommonJS regardless of package type
// =================================================================
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/package.json', JSON.stringify({ type: 'module' }));
  myVfs.writeFileSync('/helper.cjs', 'module.exports = { cjsAlways: true };');
  myVfs.writeFileSync(
    '/use-cjs.js',
    `
    import { createRequire } from 'module';
    const require = createRequire(import.meta.url);
    const result = require('/mh14/helper.cjs');
    export const cjsAlways = result.cjsAlways;
    `,
  );
  myVfs.mount('/mh14');

  const result = await import('/mh14/use-cjs.js');
  assert.strictEqual(result.cjsAlways, true);

  myVfs.unmount();
}

// =================================================================
// Test: file: URL specifier
// =================================================================
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/fileurl.mjs', 'export const fromFileUrl = true;');
  myVfs.mount('/mh15');

  const result = await import('file:///mh15/fileurl.mjs');
  assert.strictEqual(result.fromFileUrl, true);

  myVfs.unmount();
}

// =================================================================
// Test: Package with main field requiring extension resolution
// =================================================================
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
  myVfs.mount('/mh16');

  // Main field has no extension - tryExtensions should resolve entry → entry.js
  const result = require('/mh16/dir-pkg');
  assert.strictEqual(result.dirPkg, true);

  myVfs.unmount();
}

// =================================================================
// Test: Bare specifier with package subpath (no exports, direct file)
// =================================================================
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
  myVfs.mount('/mh17');

  const result = require('/mh17/app/entry-sub.js');
  assert.strictEqual(result.directSub, true);

  myVfs.unmount();
}

// =================================================================
// Test: Bare specifier subpath with extension resolution
// =================================================================
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
    // No .js extension - should be resolved by tryExtensions
    "module.exports = require('ext-pkg/lib/util');",
  );
  myVfs.mount('/mh18');

  const result = require('/mh18/app/entry-ext.js');
  assert.strictEqual(result.extSub, true);

  myVfs.unmount();
}

// =================================================================
// Test: Bare specifier main field with extension resolution
// =================================================================
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
  myVfs.mount('/mh19');

  const result = require('/mh19/app/entry-main-ext.js');
  assert.strictEqual(result.mainExt, true);

  myVfs.unmount();
}

// =================================================================
// Test: exports with array value (fallback array)
// =================================================================
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
  myVfs.mount('/mh22');

  // Array target in exports: canonical resolver tries each entry in order
  const result = require('/mh22/app/entry-arr.js');
  assert.strictEqual(result.arrExport, true);

  myVfs.unmount();
}

// =================================================================
// Test: Exports with "default" condition
// =================================================================
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
  myVfs.mount('/mh23');

  // 'browser' condition not active in Node, 'default' should match
  const result = await import('/mh23/app/entry-default.mjs');
  assert.strictEqual(result.fromDefault, true);

  myVfs.unmount();
}

// =================================================================
// Test: Package.json type "commonjs" explicitly set for .js
// =================================================================
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/package.json', JSON.stringify({ type: 'commonjs' }));
  myVfs.writeFileSync('/explicit-cjs.js', 'module.exports = { explicitCjs: true };');
  myVfs.mount('/mh24');

  const result = require('/mh24/explicit-cjs.js');
  assert.strictEqual(result.explicitCjs, true);

  myVfs.unmount();
}

// =================================================================
// Test: .js file with no package.json → defaults to commonjs
// =================================================================
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/no-pkg.js', 'module.exports = { noPkg: true };');
  myVfs.mount('/mh25');

  const result = require('/mh25/no-pkg.js');
  assert.strictEqual(result.noPkg, true);

  myVfs.unmount();
}

// =================================================================
// Test: Package.json type walk stops at node_modules boundary
// =================================================================
{
  const myVfs = vfs.create();
  // Root has type: module
  myVfs.writeFileSync('/package.json', JSON.stringify({ type: 'module' }));
  // But file is inside node_modules with no local package.json
  myVfs.mkdirSync('/node_modules/inner', { recursive: true });
  myVfs.writeFileSync(
    '/node_modules/inner/index.js',
    'module.exports = { inner: true };',
  );
  myVfs.mount('/mh26');

  // The walk should stop at node_modules, not inherit type:module from root
  const result = require('/mh26/node_modules/inner/index.js');
  assert.strictEqual(result.inner, true);

  myVfs.unmount();
}

// =================================================================
// Test: Invalid package.json in directory resolution
// =================================================================
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/bad-json-dir', { recursive: true });
  myVfs.writeFileSync('/bad-json-dir/package.json', '{ invalid json }');
  myVfs.writeFileSync(
    '/bad-json-dir/index.js',
    'module.exports = { fallbackIndex: true };',
  );
  myVfs.mount('/mh28');

  // Should fall through to index.js after failing to parse package.json
  const result = require('/mh28/bad-json-dir');
  assert.strictEqual(result.fallbackIndex, true);

  myVfs.unmount();
}

// =================================================================
// Test: Invalid package.json in type walk-up
// =================================================================
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/package.json', '{ broken json }');
  myVfs.writeFileSync('/no-type.js', 'module.exports = { noType: true };');
  myVfs.mount('/mh29');

  // Should treat as 'none' (commonjs) since package.json is invalid
  const result = require('/mh29/no-type.js');
  assert.strictEqual(result.noType, true);

  myVfs.unmount();
}

// =================================================================
// Test: Scoped package without slash (just @scope/name)
// =================================================================
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
  myVfs.mount('/mh30');

  const result = require('/mh30/app/entry-solo.js');
  assert.strictEqual(result.solo, true);

  myVfs.unmount();
}

// =================================================================
// Test: node: builtin passthrough
// =================================================================
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/use-builtin.mjs', `
    import path from 'node:path';
    export const sep = path.sep;
  `);
  myVfs.mount('/mh31');

  const result = await import('/mh31/use-builtin.mjs');
  assert.strictEqual(typeof result.sep, 'string');

  myVfs.unmount();
}

// =================================================================
// Test: JSON import with type assertion
// =================================================================
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/data.json', JSON.stringify({ preformat: true }));
  myVfs.mount('/mh32');

  const result = await import('/mh32/data.json', { with: { type: 'json' } });
  assert.strictEqual(result.default.preformat, true);

  myVfs.unmount();
}

// =================================================================
// Test: File with unknown extension → defaults to commonjs
// =================================================================
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/data.txt', 'module.exports = { txt: true };');
  myVfs.mount('/mh33');

  // .txt extension → falls back to 'commonjs' via VFS_FORMAT_MAP default
  const result = require('/mh33/data.txt');
  assert.strictEqual(result.txt, true);

  myVfs.unmount();
}
