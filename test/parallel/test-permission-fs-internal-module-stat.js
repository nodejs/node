// Flags: --expose-internals --permission --allow-fs-read=test/common* --allow-fs-read=tools* --allow-fs-read=test/parallel* --allow-child-process --allow-natives-syntax
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');
const { strictEqual } = require('assert');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

if (!common.hasCrypto) {
  common.skip('no crypto');
}

const { internalBinding } = require('internal/test/binding');
const fixtures = require('../common/fixtures');

const blockedFile = fixtures.path('permission', 'deny', 'protected-file.md');
const internalFsBinding = internalBinding('fs');

strictEqual(internalFsBinding.internalModuleStat(blockedFile), 0);
