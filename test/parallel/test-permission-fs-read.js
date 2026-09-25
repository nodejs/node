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
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
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
  const boundaryFile = path.join(tmpdir.path, 'secret');
  const grantedFiles = ['secret1', 'secret2', 'secret3']
    .map((file) => path.join(tmpdir.path, file));

  fs.writeFileSync(boundaryFile, 'protected');
  for (const file of grantedFiles) {
    fs.writeFileSync(file, 'granted');
  }

  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--permission',
      ...grantedFiles.map((file) => `--allow-fs-read=${file}`),
      ...grantedFiles.map((file) => `--allow-fs-write=${file}`),
      '-e',
      `
        const assert = require('assert');
        const fs = require('fs');
        const target = process.env.BOUNDARY_FILE;

        assert.strictEqual(process.permission.has('fs.read', target), false);
        assert.strictEqual(process.permission.has('fs.write', target), false);
        assert.throws(
          () => fs.readFileSync(target, 'utf8'),
          { code: 'ERR_ACCESS_DENIED', permission: 'FileSystemRead' }
        );
        assert.throws(
          () => fs.writeFileSync(target, 'modified'),
          { code: 'ERR_ACCESS_DENIED', permission: 'FileSystemWrite' }
        );
      `,
    ],
    {
      env: {
        ...process.env,
        BOUNDARY_FILE: boundaryFile,
      },
    }
  );
  assert.strictEqual(status, 0, stderr.toString());
  assert.strictEqual(fs.readFileSync(boundaryFile, 'utf8'), 'protected');
}

{
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--permission',
      // Do not uncomment this line
      // `--allow-fs-read=${file}`,
      `--allow-fs-read=${commonPathWildcard}`,
      file,
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
