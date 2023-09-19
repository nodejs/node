'use strict';

const common = require('../common');
common.skipIfWorker();

if (!common.hasCrypto) {
  common.skip('no crypto');
}

const { spawnSync } = require('child_process');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const file = fixtures.path('permission', 'processbinding.js');

// Due to linting rules-utils.js:isBinding check, process.binding() should
// not be called when --experimental-permission is enabled.
// Always spawn a child process
{
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--experimental-permission', '--allow-fs-read=*', file,
    ],
  );
  assert.strictEqual(status, 0, stderr.toString());
}
