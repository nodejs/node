// Tests that MODULE_NOT_FOUND errors thrown from the ESM exports resolution
// path include the requireStack property and message, matching the behaviour
// of the CJS resolution path.
// Fixes the TODO(BridgeAR) in lib/internal/modules/cjs/loader.js.
'use strict';
require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { createRequire } = require('module');
const fs = require('fs');

// --- Scenario 1: single-level require via createRequire ---
// createRequire creates a fake module at the given path. When the exports
// resolution fails, the fake module's path appears in requireStack.
const requireFixture = createRequire(fixtures.path('node_modules/'));
assert.throws(
  () => {
    requireFixture('pkgexports-esm-require-stack/missing');
  },
  (err) => {
    assert.strictEqual(err.code, 'MODULE_NOT_FOUND');

    // requireStack must be present and non-empty (previously it was undefined).
    assert(Array.isArray(err.requireStack),
           `expected err.requireStack to be an array, got ${err.requireStack}`);
    assert(err.requireStack.length > 0, 'expected err.requireStack to be non-empty');

    // The human-readable message must include the require stack section.
    assert(
      err.message.includes('Require stack:'),
      `expected error message to include "Require stack:", got:\n${err.message}`,
    );
    return true;
  },
);

// --- Scenario 2: multi-level require propagates the full requireStack ---
// When an intermediate module (inner) requires the missing export,
// requireStack must list inner first and outer second (nearest caller first).
//
// inner.js lives in test/fixtures/ so Node's normal node_modules lookup finds
// test/fixtures/node_modules/pkgexports-esm-require-stack automatically.
const innerFixture = fixtures.path('require-esm-not-found-inner.js');
const outerFixture = fixtures.path('require-esm-not-found-outer.js');

if (!fs.existsSync(innerFixture)) {
  fs.writeFileSync(
    innerFixture,
    // Uses plain require — test/fixtures/node_modules is in the search path.
    `'use strict';\nrequire('pkgexports-esm-require-stack/missing');\n`,
  );
}
if (!fs.existsSync(outerFixture)) {
  fs.writeFileSync(
    outerFixture,
    `'use strict';\nrequire(${JSON.stringify(innerFixture)});\n`,
  );
}

assert.throws(
  () => { require(outerFixture); },
  (err) => {
    assert.strictEqual(err.code, 'MODULE_NOT_FOUND');
    assert(Array.isArray(err.requireStack));

    // The inner module (direct caller of the failing require) is first.
    assert.strictEqual(err.requireStack[0], innerFixture,
                       `requireStack[0] should be ${innerFixture}, got ${err.requireStack[0]}`);
    // The outer module appears next.
    assert.strictEqual(err.requireStack[1], outerFixture,
                       `requireStack[1] should be ${outerFixture}, got ${err.requireStack[1]}`);

    // Both paths appear in the human-readable message.
    assert(err.message.includes(innerFixture));
    assert(err.message.includes(outerFixture));
    return true;
  },
);
