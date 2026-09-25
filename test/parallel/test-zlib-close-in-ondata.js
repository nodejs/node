'use strict';

const common = require('../common');
const zlib = require('zlib');

const ts = zlib.createGzip();
const buf = Buffer.alloc(1024 * 1024 * 20);

ts.on('data', common.mustCall(() => ts.close()));
ts.end(buf);
