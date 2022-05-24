'use strict';
const common = require('../common');
const assert = require('assert');

(async () => {
  await assert.rejects(import('data:text/plain,export default0'), {
    code: 'ERR_UNKNOWN_MODULE_FORMAT',
  });
  await assert.rejects(import('data:text/plain;base64,'), {
    code: 'ERR_UNKNOWN_MODULE_FORMAT',
  });
  await assert.rejects(import('data:text/css,.error { color: red; }'), {
    code: 'ERR_UNKNOWN_MODULE_FORMAT',
  });
})().then(common.mustCall());
