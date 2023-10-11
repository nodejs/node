// Flags: --expose-internals
'use strict';
require('../common');

const assert = require('assert');

const { validateAttributes } = require('internal/modules/esm/assert');

const url = 'test://';

assert.ok(validateAttributes(url, 'builtin', {}));
assert.ok(validateAttributes(url, 'commonjs', {}));
assert.ok(validateAttributes(url, 'json', { type: 'json' }));
assert.ok(validateAttributes(url, 'module', {}));
assert.ok(validateAttributes(url, 'wasm', {}));

assert.throws(() => validateAttributes(url, 'json', {}), {
  code: 'ERR_IMPORT_ASSERTION_TYPE_MISSING',
});

assert.throws(() => validateAttributes(url, 'json', { type: 'json', unsupportedAttribute: 'value' }), {
  code: 'ERR_IMPORT_ATTRIBUTE_UNSUPPORTED',
});

assert.throws(() => validateAttributes(url, 'module', { unsupportedAttribute: 'value' }), {
  code: 'ERR_IMPORT_ATTRIBUTE_UNSUPPORTED',
});

assert.throws(() => validateAttributes(url, 'module', { type: 'json' }), {
  code: 'ERR_IMPORT_ASSERTION_TYPE_FAILED',
});

// The HTML spec specifically disallows this for now, while Wasm module import
// and whether it will require a type assertion is still an open question.
assert.throws(() => validateAttributes(url, 'module', { type: 'javascript' }), {
  code: 'ERR_IMPORT_ASSERTION_TYPE_UNSUPPORTED',
});

assert.throws(() => validateAttributes(url, 'module', { type: 'css' }), {
  code: 'ERR_IMPORT_ASSERTION_TYPE_UNSUPPORTED',
});

assert.throws(() => validateAttributes(url, 'module', { type: false }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
