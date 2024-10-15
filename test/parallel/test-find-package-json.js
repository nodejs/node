'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir')
const assert = require('assert');
const { findPackageJSON } = require('module');
const { describe, it } = require('node:test');
const { pathToFileURL } = require('url');


describe('findPackageJSON', () => { // Throws when no arguments are provided
  it.skip('should throw when no arguments are provided', () => {
    assert.throws(
      () => findPackageJSON(),
      { code: 'ERR_INVALID_ARG_TYPE' }
    );
  });

  it.skip('should throw when base is invalid', () => {
    for (const invalid of ['', null, undefined, {}, [], Symbol(), 1, 0]) assert.throws(
      () => findPackageJSON('', invalid),
      { code: 'ERR_INVALID_ARG_TYPE' }
    );
  });

  it('should accept a file URL (string), like from `import.meta.resolve()`', () => {
    const importMetaUrl = `${pathToFileURL(fixtures.fixturesDir)}/`;
    const specifierBase = './packages/root-types-field/';
    assert.strictEqual(
      findPackageJSON(`${specifierBase}/index.js`, importMetaUrl),
      fixtures.path(specifierBase, 'package.json')
    );
  });
});

// { // Exclude unrecognised fields when `everything` is not `true`
//   const pathToDir = fixtures.path('packages/root-types-field/');
//   const pkg = findPackageJSON(pathToDir);

//   assert.deepStrictEqual(pkg, {
//     path: path.join(pathToDir, 'package.json'),
//     exists: true,
//     data: {
//       __proto__: null,
//       name: pkgName,
//       type: pkgType,
//     }
//   });
// }

// { // Include unrecognised fields when `everything` is `true`
//   const pathToDir = fixtures.path('packages/root-types-field/');
//   const pkg = findPackageJSON(pathToDir, true);

//   assert.deepStrictEqual(pkg, {
//     path: path.join(pathToDir, 'package.json'),
//     exists: true,
//     data: {
//       __proto__: null,
//       name: pkgName,
//       type: pkgType,
//       types: './index.d.ts',
//     }
//   });
// }

// { // Exclude unrecognised fields when `everything` is not `true`
//   const pathToDir = fixtures.path('packages/nested-types-field/');
//   const pkg = findPackageJSON(pathToDir);

//   assert.deepStrictEqual(pkg, {
//     path: path.join(pathToDir, 'package.json'),
//     exists: true,
//     data: {
//       __proto__: null,
//       name: pkgName,
//       type: pkgType,
//       exports: {
//         default: './index.js',
//         types: './index.d.ts', // I think this is unexpected?
//       },
//     },
//   });
// }

// { // Include unrecognised fields when `everything` is not `true`
//   const pathToDir = fixtures.path('packages/nested-types-field/');
//   const pkg = findPackageJSON(pathToDir, true);

//   assert.deepStrictEqual(pkg, {
//     path: path.join(pathToDir, 'package.json'),
//     exists: true,
//     data: {
//       __proto__: null,
//       name: pkgName,
//       type: pkgType,
//       exports: {
//         default: './index.js',
//         types: './index.d.ts', // I think this is unexpected?
//       },
//       unrecognised: true,
//     },
//   });
// }

// { // Throws on unresolved location
//   let err;
//   try {
//     findPackageJSON('..');
//   } catch (e) {
//     err = e;
//   }

//   assert.strictEqual(err.code, 'ERR_INVALID_ARG_VALUE');
//   assert.match(err.message, /fully resolved/);
//   assert.match(err.message, /relative/);
//   assert.match(err.message, /import\.meta\.resolve/);
//   assert.match(err.message, /path\.resolve\(__dirname/);
// }

// { // Can crawl up (CJS)
//   const pathToMod = fixtures.path('packages/nested/sub-pkg-cjs/index.js');
//   const parentPkg = require(pathToMod);

//   assert.strictEqual(parentPkg.data.name, 'package-with-sub-package');
//   const pathToParent = fixtures.path('packages/nested/package.json');
//   assert.strictEqual(parentPkg.path, pathToParent);
// }

// { // Can crawl up (ESM)
//   const pathToMod = fixtures.path('packages/nested/sub-pkg-mjs/index.js');
//   const parentPkg = require(pathToMod).default;

//   assert.strictEqual(parentPkg.data.name, 'package-with-sub-package');
//   const pathToParent = fixtures.path('packages/nested/package.json');
//   assert.strictEqual(parentPkg.path, pathToParent);
// }

// { // Can require via package.json
//   const pathToMod = fixtures.path('packages/cjs-main-no-index/other.js');
//   // require() falls back to package.json values like "main" to resolve when there is no index
//   const answer = require(pathToMod);

//   assert.strictEqual(answer, 43);
// }

// tmpdir.refresh();

// {
//   fs.writeFileSync(tmpdir.resolve('entry.mjs'), `
//     import { findPackageJSON, createRequire } from 'node:module';
//     const require = createRequire(import.meta.url);

//     const { secretNumber1 } = findPackageJSON(import.meta.resolve('pkg'), true).data;
//     const secretNumber2 = NaN; // TODO get the actual value
//     const { secretNumber3 } = findPackageJSON(import.meta.resolve('pkg2'), true).data;
//     const { secretNumber4 } = require('pkg2/package.json'); // TODO: is there a way to get that without relying on package.json being exported?
//     console.log(secretNumber1, secretNumber2, secretNumber3, secretNumber4);
//   `);

//   const secretNumber1 = Math.ceil(Math.random() * 999);
//   const secretNumber2 = Math.ceil(Math.random() * 999);
//   const secretNumber3 = Math.ceil(Math.random() * 999);
//   const secretNumber4 = Math.ceil(Math.random() * 999);

//   fs.mkdirSync(tmpdir.resolve('node_modules/pkg/subfolder'), { recursive: true });
//   fs.writeFileSync(tmpdir.resolve('node_modules/pkg/subfolder/index.js'), '');
//   fs.writeFileSync(tmpdir.resolve('node_modules/pkg/subfolder/package.json'), JSON.stringify({ type: 'module', secretNumber1 }));
//   fs.writeFileSync(tmpdir.resolve('node_modules/pkg/package.json'), JSON.stringify({ name: 'pkg', exports: './subfolder/index.js', secretNumber2 }));
//   fs.mkdirSync(tmpdir.resolve('node_modules/pkg/subfolder2'));
//   fs.writeFileSync(tmpdir.resolve('node_modules/pkg/subfolder2/package.json'), JSON.stringify({ type: 'module', secretNumber3 }));
//   fs.writeFileSync(tmpdir.resolve('node_modules/pkg/subfolder2/index.js'), '');
//   fs.mkdirSync(tmpdir.resolve('node_modules/pkg2'));
//   fs.writeFileSync(tmpdir.resolve('node_modules/pkg2/package.json'), JSON.stringify({ name: 'pkg', main: tmpdir.resolve('node_modules/pkg/subfolder2/index.js'), secretNumber4 }));


//   common.spawnPromisified(process.execPath, [tmpdir.resolve('entry.mjs')]).then(common.mustCall((result) => {
//     assert.deepStrictEqual(result, {
//       stdout: `${secretNumber1} ${secretNumber2} ${secretNumber3} ${secretNumber4}\n`,
//       stderr: '',
//       code: 0,
//       signal: null,
//     })
//   }))
// }
