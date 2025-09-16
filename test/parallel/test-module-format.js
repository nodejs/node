'use strict';

// This file tests that the `format` property is correctly set on
// `cache` entries when loading CommonJS and ES modules.

require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures.js');
const cache = require('module')._cache;
assert.strictEqual(cache[__filename].format, 'commonjs');

const list = [
  {
    filename: fixtures.path('es-modules/package-without-type/commonjs.js'),
    format: 'commonjs',
  },
  {
    filename: fixtures.path('es-modules/package-without-type/imports-commonjs.cjs'),
    format: 'commonjs',
  },
  {
    filename: fixtures.path('es-modules/package-without-type/module.js'),
    format: 'module',
  },
  {
    filename: fixtures.path('es-modules/package-without-type/imports-esm.mjs'),
    format: 'module',
  },
  {
    filename: fixtures.path('es-modules/package-type-module/esm.js'),
    format: 'module',
  },
  {
    filename: fixtures.path('es-modules/package-type-commonjs/index.js'),
    format: 'commonjs',
  },
  {
    filename: fixtures.path('es-modules/package-type-module/package.json'),
    format: 'json',
  },
  {
    filename: fixtures.path('typescript/ts/module-logger.ts'),
    format: 'module-typescript',
  },
  {
    filename: fixtures.path('typescript/mts/test-mts-export-foo.mts'),
    format: 'module-typescript',
  },
  {
    filename: fixtures.path('typescript/mts/test-module-export.ts'),
    format: 'module-typescript',
  },
  {
    filename: fixtures.path('typescript/cts/test-cts-export-foo.cts'),
    format: 'commonjs-typescript',
  },
  {
    filename: fixtures.path('typescript/cts/test-commonjs-export.ts'),
    format: 'commonjs-typescript',
  },
];

for (const { filename, format } of list) {
  assert(!cache[filename], `Expected ${filename} to not be in cache yet`);
  require(filename);
  assert.strictEqual(cache[filename].format, format, `Unexpected format for ${filename}`);
}
