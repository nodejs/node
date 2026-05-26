'use strict';

// Fixture for test/parallel/test-assert-ok-source-maps.js (CommonJS variant).
// See assert-no-source-map.mjs for the rationale (issue #63169).
const assert = require('node:assert');

try {
  assert.ok(false);
} catch (e) {
  process.stdout.write(e.constructor.name + '\n');
  process.stdout.write(e.code + '\n');
  process.stdout.write(e.message + '\n');
  process.exit(0);
}
process.exit(1);
