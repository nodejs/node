// Flags: --experimental-permission --allow-fs-read=* --allow-fs-write=* --allow-child-process
'use strict';

const common = require('../common');
common.skipIfWorker();

if (!common.hasCrypto) {
  common.skip('no crypto');
}

const assert = require('assert');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { spawnSync } = require('child_process');
const path = require('path');

const blockedFile = fixtures.path('permission', 'deny', 'protected-file.md');
const blockedFolder = tmpdir.path;
const file = fixtures.path('permission', 'fs-read.js');
const commonPathWildcard = path.join(__filename, '../../common*');
const commonPath = path.join(__filename, '../../common');

{
  tmpdir.refresh();
}

{
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--experimental-permission', `--allow-fs-read=${file}`, `--allow-fs-read=${commonPathWildcard}`, file,
    ],
    {
      env: {
        ...process.env,
        BLOCKEDFILE: blockedFile,
        BLOCKEDFOLDER: blockedFolder,
        ALLOWEDFOLDER: commonPath,
      },
    }
  );
  assert.strictEqual(status, 0, stderr.toString());
}

{
  tmpdir.refresh();
}
