'use strict';
const common = require('../common');
const assert = require('assert');
const { PassThrough } = require('stream');

const pt = new PassThrough({ highWaterMark: 0 });
pt.on('drain', common.mustCall());
assert(!pt.write('hello1'));
pt.read();
pt.read();
