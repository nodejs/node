// Flags: --expose-internals --experimental-permission --allow-fs-read=test/common* --allow-fs-read=tools* --allow-fs-read=test/parallel* --allow-child-process
'use strict';

const common = require('../common');
common.skipIfWorker();

if (!common.hasCrypto) {
  common.skip('no crypto');
}

const { internalBinding } = require('internal/test/binding');
const fixtures = require('../common/fixtures');

const blockedFile = fixtures.path('permission', 'deny', 'protected-file.md');
const internalFsBinding = internalBinding('fs');

{
  internalFsBinding.internalModuleStat(blockedFile);
}
