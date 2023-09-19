'use strict';
const common = require('../common');
const zlib = require('zlib');

// Regression test for https://github.com/nodejs/node/issues/30976
// Writes to a stream should finish even after the readable side has been ended.

const data = zlib.deflateRawSync('Welcome');

const inflate = zlib.createInflateRaw();

inflate.resume();
inflate.write(data, common.mustCall());
inflate.write(Buffer.from([0x00]), common.mustCall());
inflate.write(Buffer.from([0x00]), common.mustCall());
inflate.flush(common.mustCall());
