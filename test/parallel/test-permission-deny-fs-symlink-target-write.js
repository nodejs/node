// Flags: --experimental-permission --allow-fs-read=* --allow-fs-write=*
'use strict';

const common = require('../common');
common.skipIfWorker();
if (!common.canCreateSymLink())
  common.skip('insufficient privileges');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh(true);

const readOnlyFolder = path.join(tmpdir.path, 'read-only');
const readWriteFolder = path.join(tmpdir.path, 'read-write');
const writeOnlyFolder = path.join(tmpdir.path, 'write-only');

fs.mkdirSync(readOnlyFolder);
fs.mkdirSync(readWriteFolder);
fs.mkdirSync(writeOnlyFolder);
fs.writeFileSync(path.join(readOnlyFolder, 'file'), 'evil file contents');
fs.writeFileSync(path.join(readWriteFolder, 'file'), 'NO evil file contents');

{
  assert.ok(process.permission.deny('fs.write', [readOnlyFolder]));
  assert.ok(process.permission.deny('fs.read', [writeOnlyFolder]));
}

{
  // App won't be able to symlink from a readOnlyFolder
  assert.throws(() => {
    fs.symlink(path.join(readOnlyFolder, 'file'), path.join(readWriteFolder, 'link-to-read-only'), 'file', (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(readOnlyFolder, 'file')),
  }));

  // App will be able to symlink to a writeOnlyFolder
  fs.symlink(path.join(readWriteFolder, 'file'), path.join(writeOnlyFolder, 'link-to-read-write'), 'file', (err) => {
    assert.ifError(err);
    // App will won't be able to read the symlink
    assert.throws(() => {
      fs.readFile(path.join(writeOnlyFolder, 'link-to-read-write'), (err) => {
        assert.ifError(err);
      });
    }, common.expectsError({
      code: 'ERR_ACCESS_DENIED',
      permission: 'FileSystemRead',
    }));

    // App will be able to write to the symlink
    fs.writeFile(path.join(writeOnlyFolder, 'link-to-read-write'), 'some content', common.mustSucceed());
  });

  // App won't be able to symlink to a readOnlyFolder
  assert.throws(() => {
    fs.symlink(path.join(readWriteFolder, 'file'), path.join(readOnlyFolder, 'link-to-read-only'), 'file', (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(readOnlyFolder, 'link-to-read-only')),
  }));
}
