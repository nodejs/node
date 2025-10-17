'use strict';

require('../common');
const test = require('node:test');
const assert = require('node:assert');
const { pathToFileURL } = require('node:url');
const fixtures = require('../common/fixtures');
const { resolveLoadAndCache } = require('node:module');

const parentURL = pathToFileURL(__filename);

test('should throw if the module is not found', async () => {
  await assert.rejects(
    async () => {
      await resolveLoadAndCache('nonexistent-module', parentURL);
    },
    {
      code: 'ERR_MODULE_NOT_FOUND',
    }
  );
});

test('should load a module', async () => {
  const fileUrl = fixtures.fileURL('es-modules/cjs.js');
  const { url, format, source } = await resolveLoadAndCache(fileUrl, parentURL);
  assert.strictEqual(format, 'commonjs');
  assert.strictEqual(url, fileUrl.href);

  // `source` is null and the final builtin loader will read the file.
  assert.strictEqual(source, null);
});
