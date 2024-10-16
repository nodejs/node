'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const fs = require('node:fs');
const { findPackageJSON } = require('node:module');
const { describe, it } = require('node:test');
const { pathToFileURL } = require('node:url');


describe('findPackageJSON', () => { // Throws when no arguments are provided
  it('should throw when no arguments are provided', () => {
    assert.throws(
      () => findPackageJSON(),
      { code: 'ERR_INVALID_ARG_TYPE' }
    );
  });

  it('should throw when parentURL is invalid', () => {
    for (const invalid of ['', null, undefined, {}, [], Symbol(), 1, 0]) assert.throws(
      () => findPackageJSON('', invalid),
      { code: 'ERR_INVALID_ARG_TYPE' },
      invalid
    );
  });

  it('should accept a file URL (string), like from `import.meta.resolve()`', () => {
    const importMetaUrl = `${pathToFileURL(fixtures.fixturesDir)}/whatever.ext`;
    const specifierBase = './packages/root-types-field/';
    assert.strictEqual(
      findPackageJSON(`${specifierBase}index.js`, importMetaUrl),
      fixtures.path(specifierBase, 'package.json')
    );
  });

  it('should be able to crawl up (CJS)', () => {
    const pathToMod = fixtures.path('packages/nested/sub-pkg-cjs/index.js');
    const parentPkg = require(pathToMod);

    const pathToParent = fixtures.path('packages/nested/package.json');
    assert.strictEqual(parentPkg, pathToParent);
  });

  it('should be able to crawl up (ESM)', () => {
    const pathToMod = fixtures.path('packages/nested/sub-pkg-esm/index.js');
    const parentPkg = require(pathToMod).default; // This test is a CJS file

    const pathToParent = fixtures.path('packages/nested/package.json');
    assert.strictEqual(parentPkg, pathToParent);
  });

  it('can require via package.json', () => {
    const pathToMod = fixtures.path('packages/cjs-main-no-index/other.js');
    // `require()` falls back to package.json values like "main" to resolve when there is no index
    const answer = require(pathToMod);

    assert.strictEqual(answer, 43);
  });

  it('should be able to do whatever the heck Antoine created', () => {
    tmpdir.refresh();

    fs.writeFileSync(tmpdir.resolve('entry.mjs'), `
      import { findPackageJSON, createRequire } from 'node:module';
      import fs from 'node:fs';
      const require = createRequire(import.meta.url);

      const readPJSON = (locus) => JSON.parse(fs.readFileSync(locus, 'utf8'));

      const { secretNumber1 } = readPJSON(findPackageJSON('pkg', import.meta.url));
      // This is impossible to resolve because pkg's pjson.exports blocks access
      let secretNumber2Location = NaN;
      try { secretNumber2Location = findPackageJSON('pkg/subfolder/', import.meta.url) } catch {}
      const { secretNumber3 } = readPJSON(findPackageJSON('pkg2', import.meta.url));
      const { secretNumber4 } = readPJSON(findPackageJSON('pkg2/package.json', import.meta.url));

      console.log(secretNumber1, secretNumber2Location, secretNumber3, secretNumber4);
    `);

    const secretNumber1 = Math.ceil(Math.random() * 999);
    const secretNumber2 = Math.ceil(Math.random() * 999);
    const secretNumber3 = Math.ceil(Math.random() * 999);
    const secretNumber4 = Math.ceil(Math.random() * 999);

    fs.mkdirSync(tmpdir.resolve('node_modules/pkg/subfolder'), { recursive: true });
    fs.writeFileSync(
      tmpdir.resolve('node_modules/pkg/subfolder/index.js'),
      '',
    );
    fs.writeFileSync(
      tmpdir.resolve('node_modules/pkg/subfolder/package.json'),
      JSON.stringify({
        type: 'module',
        secretNumber1,
      }),
    );
    fs.writeFileSync(
      tmpdir.resolve('node_modules/pkg/package.json'),
      JSON.stringify({
        name: 'pkg',
        exports: './subfolder/index.js',
        secretNumber2,
      }),
    );

    fs.mkdirSync(tmpdir.resolve('node_modules/pkg/subfolder2'));
    fs.writeFileSync(
      tmpdir.resolve('node_modules/pkg/subfolder2/package.json'),
      JSON.stringify({
        type: 'module',
        secretNumber3,
      }),
    );
    fs.writeFileSync(
      tmpdir.resolve('node_modules/pkg/subfolder2/index.js'),
      '',
    );

    fs.mkdirSync(tmpdir.resolve('node_modules/pkg2'));
    fs.writeFileSync(
      tmpdir.resolve('node_modules/pkg2/package.json'),
      JSON.stringify({
        name: 'pkg',
        main: tmpdir.resolve('node_modules/pkg/subfolder2/index.js'),
        secretNumber4,
      }),
    );

    common.spawnPromisified(process.execPath, [tmpdir.resolve('entry.mjs')]).then(common.mustCall((result) => {
      console.error(result.stderr);
      console.log(result.stdout);
      assert.deepStrictEqual(result, {
        stdout: `${secretNumber1} NaN ${secretNumber3} ${secretNumber4}\n`,
        stderr: '',
        code: 0,
        signal: null,
      });
    }));
  });
});
