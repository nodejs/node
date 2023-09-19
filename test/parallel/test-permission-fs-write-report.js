// Flags: --experimental-permission --allow-fs-read=*
'use strict';

const common = require('../common');
common.skipIfWorker();
if (!common.hasCrypto)
  common.skip('no crypto');

const assert = require('assert');
const path = require('path');

{
  assert.throws(() => {
    process.report.writeReport('./secret.txt');
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.resolve('./secret.txt'),
  }));
}

{
  assert.throws(() => {
    process.report.writeReport();
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: process.cwd(),
  }));
}
