// Flags: --expose-internals
'use strict';
const common = require('../common');
const { deepStrictEqual, throws } = require('node:assert');
const { convertStringToRegExp } = require('internal/test_runner/utils');

deepStrictEqual(convertStringToRegExp('foo', 'x'), /foo/);
deepStrictEqual(convertStringToRegExp('/bar/', 'x'), /bar/);
deepStrictEqual(convertStringToRegExp('/baz/gi', 'x'), /baz/gi);
deepStrictEqual(convertStringToRegExp('/foo/9', 'x'), /\/foo\/9/);

throws(
  () => convertStringToRegExp('/foo/abcdefghijk', 'x'),
  common.expectsError({
    code: 'ERR_INVALID_ARG_VALUE',
    message: "The argument 'x' is an invalid regular expression. " +
             "Invalid flags supplied to RegExp constructor 'abcdefghijk'. " +
             "Received '/foo/abcdefghijk'",
  })
);
