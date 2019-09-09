'use strict';
const { mustNotCall } = require('../common');
const { promisify } = require('util');

const setTimeoutAsync = promisify(setTimeout);

const expected = ['foo', 'bar', 'baz'];
const promise = setTimeoutAsync(10, ...expected);
promise.then(() => mustNotCall('expected timeout to be cleared'));
clearTimeout(promise.timeout);
