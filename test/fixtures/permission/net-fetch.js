'use strict';

const common = require('../../common');
const assert = require('assert');
const url = process.env.URL;

fetch(url).catch(common.mustCall((err) => {
  assert.strictEqual(err.cause.code, 'ERR_ACCESS_DENIED');
}));
