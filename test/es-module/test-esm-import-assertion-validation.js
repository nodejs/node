// Flags: --expose-internals
'use strict';
require('../common');

const assert = require('assert');

const { validateAssertions } = require('internal/modules/esm/assert');

const url = 'test://';

assert.ok(validateAssertions(url, 'builtin', {}));
assert.ok(validateAssertions(url, 'commonjs', {}));
assert.ok(validateAssertions(url, 'json', { type: 'json' }));
assert.ok(validateAssertions(url, 'module', {}));
assert.ok(validateAssertions(url, 'wasm', {}));

assert.throws(() => validateAssertions(url, 'json', {}), {
  code: 'ERR_IMPORT_ASSERTION_TYPE_MISSING',
});

assert.throws(() => validateAssertions(url, 'module', { type: 'json' }), {
  code: 'ERR_IMPORT_ASSERTION_TYPE_FAILED',
});

// This should be allowed according to HTML spec. Let's keep it disabled
// until WASM module import is sorted out.
assert.throws(() => validateAssertions(url, 'module', { type: 'javascript' }), {
  code: 'ERR_IMPORT_ASSERTION_TYPE_UNSUPPORTED',
});

assert.throws(() => validateAssertions(url, 'module', { type: 'css' }), {
  code: 'ERR_IMPORT_ASSERTION_TYPE_UNSUPPORTED',
});

assert.throws(() => validateAssertions(url, 'module', { type: false }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
