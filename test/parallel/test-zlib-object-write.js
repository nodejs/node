'use strict';

require('../common');
const assert = require('assert');
const { Gunzip } = require('zlib');

const gunzip = new Gunzip({ objectMode: true });
gunzip.write({}, (err) => {
  assert.strictEqual(err.code, 'ERR_INVALID_ARG_TYPE');
});
gunzip.on('error', (err) => {
  assert.strictEqual(err.code, 'ERR_INVALID_ARG_TYPE');
});
