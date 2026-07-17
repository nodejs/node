// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('node:assert');
const { convertStringToRegExp } = require('internal/test_runner/utils');

assert.deepStrictEqual(convertStringToRegExp('foo', 'x'), /foo/);
assert.deepStrictEqual(convertStringToRegExp('/bar/', 'x'), /bar/);
assert.deepStrictEqual(convertStringToRegExp('/baz/gi', 'x'), /baz/gi);
assert.deepStrictEqual(convertStringToRegExp('/foo/9', 'x'), /\/foo\/9/);

assert.throws(
  () => convertStringToRegExp('/foo/abcdefghijk', 'x'),
  common.expectsError({
    code: 'ERR_INVALID_ARG_VALUE',
    message: "The argument 'x' is an invalid regular expression. " +
             "Invalid flags supplied to RegExp constructor 'abcdefghijk'. " +
             "Received '/foo/abcdefghijk'",
  })
);
