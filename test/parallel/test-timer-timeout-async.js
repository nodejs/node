'use strict';
require('../common');
const { strictEqual } = require('assert');
const { promisify } = require('util');

const setTimeoutAsync = promisify(setTimeout);

const expected = ['foo', 'bar', 'baz'];
// N.B. the promisified version of setTimeout will resolve with the _first_
// value, not an array of all values.
setTimeoutAsync(10, ...expected)
  .then((actual) => strictEqual(actual, expected[0]));
