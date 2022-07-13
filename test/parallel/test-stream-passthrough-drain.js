'use strict';
const common = require('../common');
const { PassThrough } = require('stream');

const pt = new PassThrough({ highWaterMark: 0 });
pt.on('drain', common.mustCall());
pt.write('hello');
pt.read();
