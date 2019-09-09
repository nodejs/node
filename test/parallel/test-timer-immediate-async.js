'use strict';
require('../common');
const { strictEqual } = require('assert');
const { promisify } = require('util');

const setImmediateAsync = promisify(setImmediate);

const expected = ['foo', 'bar', 'baz'];
// N.B. the promisified version of setImmediate will resolve with the _first_
// value, not an array of all values.
setImmediateAsync(...expected)
  .then((actual) => strictEqual(actual, expected[0]));
