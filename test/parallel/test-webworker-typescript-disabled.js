// Flags: --experimental-web-worker --no-strip-types
'use strict';

const common = require('../common');
// Disabling stripping must reject annotations, not JavaScript-compatible .ts files.
const assert = require('node:assert');
const { writeFileSync } = require('node:fs');
const { join } = require('node:path');
const { pathToFileURL } = require('node:url');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const worker = new Worker(fixtures.fileURL('web-worker', 'typescript', 'entry.ts'), { type: 'module' });
worker.onmessage = common.mustNotCall('types must not be stripped');
worker.onerror = common.mustCall(({ error }) => {
  assert.strictEqual(error.name, 'SyntaxError');
});

const path = join(tmpdir.path, 'entry.ts');
writeFileSync(path, 'postMessage(1);');
const untyped = new Worker(pathToFileURL(path), { type: 'module' });
untyped.onerror = common.mustNotCall('JavaScript-compatible entries must still work');
untyped.onmessage = common.mustCall(({ data }) => {
  assert.strictEqual(data, 1);
  untyped.terminate();
});
