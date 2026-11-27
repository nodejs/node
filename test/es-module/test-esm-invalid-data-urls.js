'use strict';
const common = require('../common');
const assert = require('assert');

(async () => {
  await assert.rejects(import('data:text/plain,export default0'), {
    code: 'ERR_IMPORT_ATTRIBUTE_MISSING',
    message:
      'Module "data:text/plain,export default0" needs an import attribute ' +
      'of "type: text"',
  });
  await assert.rejects(import('data:text/plain;base64,'), {
    code: 'ERR_IMPORT_ATTRIBUTE_MISSING',
    message:
      'Module "data:text/plain;base64," needs an import attribute of ' +
      '"type: text"',
  });
  await assert.rejects(import('data:text/css,.error { color: red; }'), {
    code: 'ERR_UNKNOWN_MODULE_FORMAT',
    message: 'Unknown module format: text/css for URL data:text/css,.error { color: red; }',
  });
  await assert.rejects(import('data:WRONGtext/javascriptFORMAT,console.log("hello!");'), {
    code: 'ERR_UNKNOWN_MODULE_FORMAT',
  });
})().then(common.mustCall());
