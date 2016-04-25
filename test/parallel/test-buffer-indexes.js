'use strict';
const assert = require('assert');
const common = require('../common');

const TEST_BUF = Buffer.from('Hello World, the day of tomorrows past');

assert(common);

assert(TEST_BUF.indexes(new Buffer('or')).length === 2);
assert(TEST_BUF.indexes(new Buffer('o')).length === 6);
assert(TEST_BUF.indexes(new Buffer('k')).length === 0);
assert(TEST_BUF.indexes(new Buffer('')).length === 0);

assert(new Buffer('short').indexes(new Buffer('veeeerylong')).length === 0);
assert(new Buffer('short').indexes(new Buffer('shortnotshort')).length === 0);
