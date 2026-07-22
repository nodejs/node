'use strict';

const common = require('../common');
const assert = require('assert');
const { Gunzip } = require('zlib');

const gunzip = new Gunzip({ objectMode: true });
gunzip.on('error', common.mustNotCall());
assert.throws(() => {
  gunzip.write({});
}, {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE'
});
