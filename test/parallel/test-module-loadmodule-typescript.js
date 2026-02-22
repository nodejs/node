// Flags: --experimental-strip-types

'use strict';

require('../common');
const test = require('node:test');
const assert = require('node:assert');
const { pathToFileURL } = require('node:url');
const fixtures = require('../common/fixtures');
const { resolveLoadAndCache } = require('node:module');

const parentURL = pathToFileURL(__filename);

test('should load a TypeScript module source by package.json type', async () => {
  // Even if the .ts file contains module syntax, it should be loaded as a CommonJS module
  // because the package.json type is set to "commonjs".

  const fileUrl = fixtures.fileURL('typescript/legacy-module/test-module-export.ts');
  const { url, format, source } = await resolveLoadAndCache(fileUrl, parentURL);
  assert.strictEqual(format, 'commonjs-typescript');
  assert.strictEqual(url, fileUrl.href);

  // Built-in TypeScript loader loads the source.
  assert.ok(Buffer.isBuffer(source));
});

test('should load a TypeScript cts module source by extension', async () => {
  // By extension, .cts files should be loaded as CommonJS modules.

  const fileUrl = fixtures.fileURL('typescript/legacy-module/test-module-export.cts');
  const { url, format, source } = await resolveLoadAndCache(fileUrl, parentURL);
  assert.strictEqual(format, 'commonjs-typescript');
  assert.strictEqual(url, fileUrl.href);

  // Built-in TypeScript loader loads the source.
  assert.ok(Buffer.isBuffer(source));
});

test('should load a TypeScript mts module source by extension', async () => {
  // By extension, .mts files should be loaded as ES modules.

  const fileUrl = fixtures.fileURL('typescript/legacy-module/test-module-export.mts');
  const { url, format, source } = await resolveLoadAndCache(fileUrl, parentURL);
  assert.strictEqual(format, 'module-typescript');
  assert.strictEqual(url, fileUrl.href);

  // Built-in TypeScript loader loads the source.
  assert.ok(Buffer.isBuffer(source));
});
