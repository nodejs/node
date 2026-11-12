// Tests that a CommonJS module left over from an import() that failed to link
// uses the exports of its require.cache entry when replaced before evaluation.

import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { createRequire, Module } from 'node:module';

const require = createRequire(import.meta.url);

await assert.rejects(
  import(fixtures.fileURL('es-modules', 'cjs-after-failed-import', 'entry.mjs')),
  { code: 'ERR_IMPORT_ATTRIBUTE_MISSING' },
);

const depPath = fixtures.path('es-modules', 'cjs-after-failed-import', 'dep.cjs');
const replacement = new Module(depPath);
replacement.exports = { name: 'replacement' };
replacement.loaded = true;
require.cache[depPath] = replacement;
const { default: dep } = await import(fixtures.fileURL('es-modules', 'cjs-after-failed-import', 'importer.mjs'));
assert.strictEqual(dep, replacement.exports);
