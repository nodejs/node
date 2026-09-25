'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { buildSnapshot, runWithSnapshot } = require('../common/snapshot');

const entry = fixtures.path('snapshot', 'crypto-random-cache.js');

tmpdir.refresh();
for (const mode of [
  'uuid', 'uuidv7', 'object-url', 'async', 'serialize',
  'microtask', 'next-tick', 'exit', 'unprimed',
]) {
  buildSnapshot(entry, { NODE_TEST_CRYPTO_RANDOM_MODE: mode });
  const a = JSON.parse(runWithSnapshot().stdout);
  const b = JSON.parse(runWithSnapshot().stdout);

  // Values retained by the application at build time are intentionally shared.
  assert.deepStrictEqual(a.buildValues, b.buildValues);
  for (const phase of ['deserialized', 'main']) {
    for (const api of Object.keys(a[phase])) {
      assert.notDeepStrictEqual(a[phase][api], b[phase][api], `${mode}: ${phase} ${api}`);
    }
  }
}
