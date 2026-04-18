// Flags: --permission --allow-fs-read=* --allow-fs-write=* --allow-child-process
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const fixtures = require('../common/fixtures');
if (!common.canCreateSymLink()) {
  common.skip('insufficient privileges');
}
if (!common.hasCrypto) {
  common.skip('no crypto');
}

const assert = require('assert');
const fs = require('fs');
const { spawnSync } = require('child_process');
const path = require('path');
const tmpdir = require('../common/tmpdir');

const file = fixtures.path('permission', 'fs-symlink.js');
const commonPathWildcard = path.join(__filename, '../../common*');
const blockedFile = fixtures.path('permission', 'deny', 'protected-file.md');
const blockedFolder = tmpdir.resolve('subdirectory');
const symlinkFromBlockedFile = tmpdir.resolve('example-symlink.md');
const allowedFolder = tmpdir.resolve('allowed-folder');
const traversalSymlink = path.join(allowedFolder, 'deep1', 'deep2', 'deep3', 'gotcha');

{
  tmpdir.refresh();
  fs.mkdirSync(blockedFolder);
  // Create deep directory structure for path traversal test
  fs.mkdirSync(allowedFolder);
  fs.writeFileSync(path.resolve(allowedFolder, '../protected-file.md'), 'protected');
  fs.mkdirSync(path.join(allowedFolder, 'deep1'));
  fs.mkdirSync(path.join(allowedFolder, 'deep1', 'deep2'));
  fs.mkdirSync(path.join(allowedFolder, 'deep1', 'deep2', 'deep3'));
}

{
  // Symlink previously created
  // fs.symlink API is allowed when full-read and full-write access
  fs.symlinkSync(blockedFile, symlinkFromBlockedFile);
  // Create symlink for path traversal test - symlink points to parent directory
  fs.symlinkSync(allowedFolder, traversalSymlink);
}

{
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--permission',
      `--allow-fs-read=${file}`, `--allow-fs-read=${commonPathWildcard}`, `--allow-fs-read=${symlinkFromBlockedFile}`,
      `--allow-fs-read=${allowedFolder}`,
      `--allow-fs-write=${symlinkFromBlockedFile}`,
      file,
    ],
    {
      env: {
        ...process.env,
        BLOCKEDFOLDER: blockedFolder,
        BLOCKEDFILE: blockedFile,
        EXISTINGSYMLINK: symlinkFromBlockedFile,
        TRAVERSALSYMLINK: traversalSymlink,
        ALLOWEDFOLDER: allowedFolder,
      },
    }
  );
  assert.strictEqual(status, 0, stderr.toString());
}

{
  tmpdir.refresh();
}
