// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
const assert = require('node:assert');

assert.strictEqual(process.permission.has('addon'), false);

assert.throws(() => {
  process._linkedBinding('missing');
}, common.expectsError({
  code: 'ERR_ACCESS_DENIED',
  permission: 'Addon',
  resource: 'missing',
}));
