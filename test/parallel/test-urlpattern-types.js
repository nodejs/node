'use strict';

require('../common');

const { URLPattern } = require('url');
const assert = require('assert');

// Verifies that calling URLPattern with no new keyword throws.
assert.throws(() => URLPattern(), {
  code: 'ERR_CONSTRUCT_CALL_REQUIRED',
});

// Verifies that type checks are performed on the arguments.
assert.throws(() => new URLPattern(1), {
  code: 'ERR_INVALID_ARG_TYPE',
});

assert.throws(() => new URLPattern({}, 1), {
  code: 'ERR_INVALID_ARG_TYPE',
});

assert.throws(() => new URLPattern({}, '', 1), {
  code: 'ERR_INVALID_ARG_TYPE',
});

assert.throws(() => new URLPattern({}, { ignoreCase: '' }), {
  code: 'ERR_INVALID_ARG_TYPE',
});

const pattern = new URLPattern();

assert.throws(() => pattern.exec(1), {
  code: 'ERR_INVALID_ARG_TYPE',
});

assert.throws(() => pattern.exec('', 1), {
  code: 'ERR_INVALID_ARG_TYPE',
});

assert.throws(() => pattern.test(1), {
  code: 'ERR_INVALID_ARG_TYPE',
});

assert.throws(() => pattern.test('', 1), {
  code: 'ERR_INVALID_ARG_TYPE',
});
