'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

// Skip if ESM is not supported in this environment
common.skipIfESMUnsupported(__filename);

// Fixture that mixes CJS + ESM
const entry = fixtures.path('es-modules', 'ambiguous-mixed-syntax.js');

const result = spawnSync(process.execPath, [entry], {
  encoding: 'utf8'
});

// Node should exit with an error
assert.notStrictEqual(result.status, 0);

// Error code must be present
assert.ok(
  /ERR_AMBIGUOUS_MODULE_SYNTAX/.test(result.stderr),
  `stderr did not contain ERR_AMBIGUOUS_MODULE_SYNTAX:\n${result.stderr}`
);

// Your new explanatory message should appear
assert.ok(
  /mixes CommonJS/.test(result.stderr),
  `stderr did not contain expected message:\n${result.stderr}`
);
