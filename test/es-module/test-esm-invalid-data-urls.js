'use strict';
const common = require('../common');
const assert = require('assert');

(async () => {
  await assert.rejects(import('data:text/plain,export default0'), {
    code: 'ERR_INVALID_MODULE_SPECIFIER',
    message:
      'Invalid module "data:text/plain,export default0" has an unsupported ' +
      'MIME type "text/plain"',
  });
  await assert.rejects(import('data:text/plain;base64,'), {
    code: 'ERR_INVALID_MODULE_SPECIFIER',
    message:
      'Invalid module "data:text/plain;base64," has an unsupported ' +
      'MIME type "text/plain"',
  });
  await assert.rejects(import('data:application/json,[]'), {
    code: 'ERR_INVALID_MODULE_SPECIFIER',
    message:
      'Invalid module "data:application/json,[]" has an unsupported ' +
      'MIME type "application/json"',
  });
})().then(common.mustCall());
