// Flags: --permission --allow-fs-read=* --allow-fs-write=* --allow-child-process
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

if (!common.hasCrypto) {
  common.skip('no crypto');
}

const assert = require('assert');
const fixtures = require('../common/fixtures');
const { spawnSync } = require('child_process');

const file = fixtures.path('permission', 'hello-world.js');
const simpleLoader = fixtures.path('permission', 'simple-loader.js');
const fsReadLoader = fixtures.path('permission', 'fs-read-loader.js');

[
  '',
  simpleLoader,
  fsReadLoader,
].forEach((arg0) => {
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      arg0 !== '' ? '-r' : '',
      arg0,
      '--permission',
      file,
    ],
  );
  assert.strictEqual(status, 0, `${arg0} Error: ${stderr.toString()}`);
});
