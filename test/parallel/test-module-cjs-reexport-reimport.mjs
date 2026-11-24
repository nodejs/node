// Regression test for https://github.com/nodejs/node/issues/67611
// Re-importing a CommonJS module whose exports are a re-export
// (module.exports = require('./dep.cjs')) after both modules were deleted
// from require.cache must re-evaluate the dependency instead of returning
// the empty exports of the unloaded cache entry pre-created by the ESM
// loader for the re-export.

import '../common/index.mjs';
import assert from 'node:assert';
import { realpathSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const require = createRequire(import.meta.url);
const dir = realpathSync(tmpdir.path);
const entry = join(dir, 'entry.cjs');
const dep = join(dir, 'dep.cjs');

writeFileSync(entry, "module.exports = require('./dep.cjs')\n");

for (const value of [1, 2, 3]) {
  writeFileSync(dep, `module.exports = { value: ${value} }\n`);
  delete require.cache[entry];
  delete require.cache[dep];
  const { default: entryExports } =
    await import(`${pathToFileURL(entry)}?v=${value}`);
  assert.deepStrictEqual(entryExports, { value });
}

// A syntax error in the re-exported dependency must surface on re-import,
// not be swallowed into empty exports.
writeFileSync(dep, 'module.exports = {\n');
delete require.cache[entry];
delete require.cache[dep];
await assert.rejects(
  import(`${pathToFileURL(entry)}?v=4`),
  SyntaxError,
);
