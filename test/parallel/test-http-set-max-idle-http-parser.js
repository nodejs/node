'use strict';
require('../common');
const assert = require('assert');
const httpCommon = require('_http_common');
const http = require('http');

[Symbol(), {}, [], () => {}, 1n, true, '1', null, undefined].forEach((value) => {
  assert.throws(() => http.setMaxIdleHTTPParsers(value), { code: 'ERR_INVALID_ARG_TYPE' });
});

[-1, -Infinity, NaN, 0, 1.1].forEach((value) => {
  assert.throws(() => http.setMaxIdleHTTPParsers(value), { code: 'ERR_OUT_OF_RANGE' });
});

[1, Number.MAX_SAFE_INTEGER].forEach((value) => {
  assert.notStrictEqual(httpCommon.parsers.max, value);
  http.setMaxIdleHTTPParsers(value);
  assert.strictEqual(httpCommon.parsers.max, value);
});
