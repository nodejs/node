// Flags: --experimental-permission --allow-fs-read=* --allow-fs-write=* --allow-child-process
'use strict';

const common = require('../common');
common.skipIfWorker();

const fixtures = require('../common/fixtures');
if (!common.canCreateSymLink())
  common.skip('insufficient privileges');
if (!common.hasCrypto)
  common.skip('no crypto');

const assert = require('assert');
const fs = require('fs');
const { spawnSync } = require('child_process');
const path = require('path');
const tmpdir = require('../common/tmpdir');

const file = fixtures.path('permission', 'fs-traversal.js');
const blockedFolder = tmpdir.path;
const allowedFolder = tmpdir.resolve('subdirectory');
const commonPathWildcard = path.join(__filename, '../../common*');

{
  tmpdir.refresh();
  fs.mkdirSync(allowedFolder);
}

{
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--experimental-permission',
      `--allow-fs-read=${file}`, `--allow-fs-read=${commonPathWildcard}`, `--allow-fs-read=${allowedFolder}`,
      `--allow-fs-write=${allowedFolder}`,
      file,
    ],
    {
      env: {
        ...process.env,
        BLOCKEDFOLDER: blockedFolder,
        ALLOWEDFOLDER: allowedFolder,
      },
    }
  );
  assert.strictEqual(status, 0, stderr.toString());
}

{
  tmpdir.refresh();
}
