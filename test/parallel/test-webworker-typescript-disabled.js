// Flags: --experimental-web-worker --no-strip-types
'use strict';

const common = require('../common');
const assert = require('node:assert');
const fixtures = require('../common/fixtures');

// Without type stripping, annotated `.ts` entries fail to parse.
const worker = new Worker(fixtures.fileURL('web-worker', 'typescript', 'entry.ts'), { type: 'module' });
worker.onmessage = common.mustNotCall('types must not be stripped');
worker.onerror = common.mustCall(({ error }) => {
  assert.strictEqual(error.name, 'SyntaxError');
});
