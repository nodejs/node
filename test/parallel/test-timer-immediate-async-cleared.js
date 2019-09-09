'use strict';
const { mustNotCall } = require('../common');
const { promisify } = require('util');

const setImmediateAsync = promisify(setImmediate);

const expected = ['foo', 'bar', 'baz'];
const promise = setImmediateAsync(...expected);
promise.then(() => mustNotCall('expected immediate to be cleared'));
clearImmediate(promise.immediate);
