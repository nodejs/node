'use strict';
const common = require('../common');
const assert = require('assert');

assert.throws(
  () => require('test'),
  common.expectsError({ code: 'ERR_UNKNOWN_BUILTIN_MODULE' }),
);

(async () => {
  await assert.rejects(
    async () => import('test'),
    common.expectsError({ code: 'ERR_UNKNOWN_BUILTIN_MODULE' }),
  );
})().then(common.mustCall());
