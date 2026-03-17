// Flags: --expose-internals --experimental-import-text
'use strict';
require('../common');

const assert = require('assert');

const { validateAttributes } = require('internal/modules/esm/assert');

const url = 'test://';

assert.ok(validateAttributes(url, 'text', { type: 'text' }));

assert.throws(() => validateAttributes(url, 'text', {}), {
  code: 'ERR_IMPORT_ATTRIBUTE_MISSING',
});

assert.throws(() => validateAttributes(url, 'module', { type: 'text' }), {
  code: 'ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE',
});
