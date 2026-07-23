// Flags: --no-experimental-strip-types

'use strict';

require('../common');
const test = require('node:test');
const assert = require('node:assert');
const { pathToFileURL } = require('node:url');
const fixtures = require('../common/fixtures');
const { resolveLoadAndCache } = require('node:module');

const parentURL = pathToFileURL(__filename);

test('should reject a TypeScript module', async () => {
  const fileUrl = fixtures.fileURL('typescript/legacy-module/test-module-export.ts');
  await assert.rejects(
    async () => {
      await resolveLoadAndCache(fileUrl, parentURL);
    },
    {
      code: 'ERR_UNKNOWN_FILE_EXTENSION',
    }
  );
});
