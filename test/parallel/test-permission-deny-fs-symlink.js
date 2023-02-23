// Flags: --experimental-permission --allow-fs-read=* --allow-fs-write=*
'use strict';

const common = require('../common');
common.skipIfWorker();
const fixtures = require('../common/fixtures');
if (!common.canCreateSymLink())
  common.skip('insufficient privileges');

const assert = require('assert');
const fs = require('fs');

const path = require('path');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh(true);

const blockedFile = fixtures.path('permission', 'deny', 'protected-file.md');
const blockedFolder = path.join(tmpdir.path, 'subdirectory');
const regularFile = __filename;
const symlinkFromBlockedFile = path.join(tmpdir.path, 'example-symlink.md');

fs.mkdirSync(blockedFolder);

{
  // Symlink previously created
  fs.symlinkSync(blockedFile, symlinkFromBlockedFile);
  assert.ok(process.permission.deny('fs.read', [blockedFile, blockedFolder]));
  assert.ok(process.permission.deny('fs.write', [blockedFile, blockedFolder]));
}

{
  // Previously created symlink are NOT affected by the permission model
  const linkData = fs.readlinkSync(symlinkFromBlockedFile);
  assert.ok(linkData);
  const fileData = fs.readFileSync(symlinkFromBlockedFile);
  assert.ok(fileData);
  // cleanup
  fs.unlink(symlinkFromBlockedFile, (err) => {
    assert.ifError(
      err,
      `Error while removing the symlink: ${symlinkFromBlockedFile}.
      You may need to remove it manually to re-run the tests`
    );
  });
}

{
  // App doesn’t have access to the BLOCKFOLDER
  assert.throws(() => {
    fs.opendir(blockedFolder, (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
  assert.throws(() => {
    fs.writeFile(blockedFolder + '/new-file', 'data', (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  }));

  // App doesn’t have access to the BLOCKEDFILE folder
  assert.throws(() => {
    fs.readFile(blockedFile, (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
  assert.throws(() => {
    fs.appendFile(blockedFile, 'data', (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  }));

  // App won't be able to symlink REGULARFILE to BLOCKFOLDER/asdf
  assert.throws(() => {
    fs.symlink(regularFile, blockedFolder + '/asdf', 'file', (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  }));

  // App won't be able to symlink BLOCKEDFILE to REGULARDIR
  assert.throws(() => {
    fs.symlink(blockedFile, path.join(__dirname, '/asdf'), 'file', (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
}
tmpdir.refresh(true);
