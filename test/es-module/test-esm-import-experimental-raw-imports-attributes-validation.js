// Flags: --expose-internals --experimental-raw-imports
'use strict';
require('../common');

const assert = require('assert');

const { validateAttributes } = require('internal/modules/esm/assert');

const url = 'test://';

// ... {type = 'text'}
assert.ok(validateAttributes(url, 'text', { type: 'text' }));

assert.throws(() => validateAttributes(url, 'text', {}), {
  code: 'ERR_IMPORT_ATTRIBUTE_MISSING',
});

assert.throws(() => validateAttributes(url, 'module', { type: 'text' }), {
  code: 'ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE',
});

// ... {type = 'bytes'}
assert.ok(validateAttributes(url, 'bytes', { type: 'bytes' }));

assert.throws(() => validateAttributes(url, 'bytes', {}), {
  code: 'ERR_IMPORT_ATTRIBUTE_MISSING',
});

assert.throws(() => validateAttributes(url, 'module', { type: 'bytes' }), {
  code: 'ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE',
});
