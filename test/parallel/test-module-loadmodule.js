'use strict';

require('../common');
const test = require('node:test');
const assert = require('node:assert');
const { pathToFileURL } = require('node:url');
const fixtures = require('../common/fixtures');
const { loadModule, kModuleFormats } = require('node:module');

const parentURL = pathToFileURL(__filename);

test('kModuleFormats is a frozen object', () => {
  assert.ok(typeof kModuleFormats === 'object');
  assert.ok(Object.isFrozen(kModuleFormats));
});

test('should throw if the module is not found', async () => {
  await assert.rejects(
    async () => {
      await loadModule('nonexistent-module', parentURL);
    },
    {
      code: 'ERR_MODULE_NOT_FOUND',
    }
  );
});

test('should load a module', async () => {
  const fileUrl = fixtures.fileURL('es-modules/cjs.js');
  const { url, format, source } = await loadModule(fileUrl, parentURL);
  assert.strictEqual(format, kModuleFormats.commonjs);
  assert.strictEqual(url, fileUrl.href);

  // `source` is null and the final builtin loader will read the file.
  assert.strictEqual(source, null);
});
