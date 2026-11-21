// Tests that a CommonJS module left over from an import() that failed to link
// is loaded afresh when its require.cache entry is deleted before evaluation.

import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);

await assert.rejects(
  import(fixtures.fileURL('es-modules', 'cjs-after-failed-import', 'entry.mjs')),
  { code: 'ERR_IMPORT_ATTRIBUTE_MISSING' },
);

const depPath = fixtures.path('es-modules', 'cjs-after-failed-import', 'dep.cjs');
delete require.cache[depPath];
const { default: dep } = await import(fixtures.fileURL('es-modules', 'cjs-after-failed-import', 'importer.mjs'));
assert.deepStrictEqual(dep, { name: 'dep' });
assert.strictEqual(require(depPath), dep);
