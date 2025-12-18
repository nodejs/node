'use strict';
const common = require('../common');
const assert = require('assert');

(async () => {
  await assert.rejects(import('data:text/plain,export default0'), {
    code: 'ERR_UNKNOWN_MODULE_FORMAT',
    message:
      'Unknown module format: text/plain for URL data:text/plain,' +
      'export default0',
  });
  await assert.rejects(import('data:text/plain;base64,'), {
    code: 'ERR_UNKNOWN_MODULE_FORMAT',
    message:
      'Unknown module format: text/plain for URL data:text/plain;base64,',
  });
  await assert.rejects(import('data:text/css,.error { color: red; }'), {
    code: 'ERR_UNKNOWN_MODULE_FORMAT',
    message: 'Unknown module format: text/css for URL data:text/css,.error { color: red; }',
  });
  await assert.rejects(import('data:WRONGtext/javascriptFORMAT,console.log("hello!");'), {
    code: 'ERR_UNKNOWN_MODULE_FORMAT',
  });
})().then(common.mustCall());
