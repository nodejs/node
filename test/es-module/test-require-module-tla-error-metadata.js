// Flags: --experimental-print-required-tla
'use strict';

// Tests that ERR_REQUIRE_ASYNC_MODULE carries metadata about the failure
// when --experimental-print-required-tla is enabled.

require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const entrypoint = fixtures.path('es-modules/tla/require-indented.js');
const url = fixtures.fileURL('es-modules/tla/tla-indented.mjs').href;

assert.throws(() => {
  require(entrypoint);
}, (err) => {
  assert.strictEqual(err.code, 'ERR_REQUIRE_ASYNC_MODULE');
  assert.deepStrictEqual(err.requireStack, [entrypoint, __filename]);
  assert.deepStrictEqual(err.topLevelAwaitLocations, [
    { __proto__: null, url, line: 4, column: 3, sourceLine: '  await Promise.resolve(ready);' },
    { __proto__: null, url, line: 7, column: 1, sourceLine: 'for await (const x of [Promise.resolve(1)]) {' },
  ]);
  // The properties are not enumerable so they do not show up in the inspected error.
  assert.deepStrictEqual(Object.keys(err), ['code']);
  return true;
});
