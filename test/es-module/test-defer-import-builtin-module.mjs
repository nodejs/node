// Flags: --js-defer-import-eval --expose-internals

// Test that uses import.defer for a builtin module. Currently
// defer importing of a synthetic module should be a no-op
// in Node.js, so the test is mostly a smoke test that Node
// doesn't crash.

import '../common/index.mjs';
import * as assert from 'assert';

import binding from 'internal/test/binding';
const { kEvaluated } = binding.internalBinding('module_wrap');
const helpers = await import('internal/modules/helpers');

// Load the http builtin module and check that it is evaluated.
let builtin = helpers.default.loadBuiltinModule('http');
let wrap = builtin.getESMFacade();
assert.strictEqual(wrap.getStatus(), kEvaluated);

// Import the http builtin module.
import defer * as http from 'node:http';
assert.notStrictEqual(http.STATUS_CODES, undefined);

// Refresh the references to the builtin module and check again its status.
builtin = helpers.default.loadBuiltinModule('http');
wrap = builtin.getESMFacade();
assert.strictEqual(wrap.getStatus(), kEvaluated);

// Check that the imported module contains some known properties.
assert.notStrictEqual(http.STATUS_CODES, undefined);
assert.notStrictEqual(http.createServer, undefined);
assert.strictEqual(typeof http.createServer, 'function');
