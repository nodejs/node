// Flags: --experimental-permission --allow-fs-read=* --allow-fs-write=*
'use strict';

const common = require('../common');
common.skipIfWorker();

const assert = require('assert');
const fs = require('fs');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const path = require('path');
const os = require('os');

const blockedFile = fixtures.path('permission', 'deny', 'protected-file.md');
const relativeProtectedFile = './test/fixtures/permission/deny/protected-file.md';
const absoluteProtectedFile = path.resolve(relativeProtectedFile);
const blockedFolder = tmpdir.path;
const regularFile = __filename;
const uid = os.userInfo().uid;
const gid = os.userInfo().gid;

{
  tmpdir.refresh();
  assert.ok(process.permission.deny('fs.read', [blockedFile, blockedFolder]));
}

// fs.readFile
{
  assert.throws(() => {
    fs.readFile(blockedFile, () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFile),
  }));
  assert.throws(() => {
    fs.readFile(relativeProtectedFile, () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(absoluteProtectedFile),
  }));
  assert.throws(() => {
    fs.readFile(path.join(blockedFolder, 'anyfile'), () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(path.join(blockedFolder, 'anyfile')),
  }));

  // doesNotThrow
  fs.readFile(regularFile, () => {});
}

// fs.createReadStream
{
  assert.rejects(() => {
    return new Promise((_resolve, reject) => {
      const stream = fs.createReadStream(blockedFile);
      stream.on('error', reject);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFile),
  })).then(common.mustCall());

  assert.rejects(() => {
    return new Promise((_resolve, reject) => {
      const stream = fs.createReadStream(relativeProtectedFile);
      stream.on('error', reject);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(absoluteProtectedFile),
  })).then(common.mustCall());

  assert.rejects(() => {
    return new Promise((_resolve, reject) => {
      const stream = fs.createReadStream(blockedFile);
      stream.on('error', reject);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFile),
  })).then(common.mustCall());
}

// fs.stat
{
  assert.throws(() => {
    fs.stat(blockedFile, () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFile),
  }));
  assert.throws(() => {
    fs.stat(relativeProtectedFile, () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(absoluteProtectedFile),
  }));
  assert.throws(() => {
    fs.stat(path.join(blockedFolder, 'anyfile'), () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(path.join(blockedFolder, 'anyfile')),
  }));

  // doesNotThrow
  fs.stat(regularFile, (err) => {
    assert.ifError(err);
  });
}

// fs.access
{
  assert.throws(() => {
    fs.access(blockedFile, fs.constants.R_OK, () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFile),
  }));
  assert.throws(() => {
    fs.access(relativeProtectedFile, fs.constants.R_OK, () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(absoluteProtectedFile),
  }));
  assert.throws(() => {
    fs.access(path.join(blockedFolder, 'anyfile'), fs.constants.R_OK, () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(path.join(blockedFolder, 'anyfile')),
  }));

  // doesNotThrow
  fs.access(regularFile, fs.constants.R_OK, (err) => {
    assert.ifError(err);
  });
}

// fs.chownSync (should not bypass)
{
  assert.throws(() => {
    // This operation will work fine
    fs.chownSync(blockedFile, uid, gid);
    fs.readFileSync(blockedFile);
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFile),
  }));
  assert.throws(() => {
    // This operation will work fine
    fs.chownSync(relativeProtectedFile, uid, gid);
    fs.readFileSync(relativeProtectedFile);
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(absoluteProtectedFile),
  }));
}

// fs.copyFile
{
  assert.throws(() => {
    fs.copyFile(blockedFile, path.join(blockedFolder, 'any-other-file'), () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFile),
  }));
  assert.throws(() => {
    fs.copyFile(relativeProtectedFile, path.join(blockedFolder, 'any-other-file'), () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(absoluteProtectedFile),
  }));
  assert.throws(() => {
    fs.copyFile(blockedFile, path.join(__dirname, 'any-other-file'), () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFile),
  }));
}

// fs.cp
{
  assert.throws(() => {
    fs.cpSync(blockedFile, path.join(blockedFolder, 'any-other-file'));
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    // cpSync calls statSync before reading blockedFile
    resource: path.toNamespacedPath(blockedFolder),
  }));
  assert.throws(() => {
    fs.cpSync(relativeProtectedFile, path.join(blockedFolder, 'any-other-file'));
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFolder),
  }));
  assert.throws(() => {
    fs.cpSync(blockedFile, path.join(__dirname, 'any-other-file'));
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFile),
  }));
}

// fs.open
{
  assert.throws(() => {
    fs.open(blockedFile, 'r', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFile),
  }));
  assert.throws(() => {
    fs.open(relativeProtectedFile, 'r', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(absoluteProtectedFile),
  }));
  assert.throws(() => {
    fs.open(path.join(blockedFolder, 'anyfile'), 'r', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(path.join(blockedFolder, 'anyfile')),
  }));

  // doesNotThrow
  fs.open(regularFile, 'r', (err) => {
    assert.ifError(err);
  });
}

// fs.opendir
{
  assert.throws(() => {
    fs.opendir(blockedFolder, (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFolder),
  }));
  // doesNotThrow
  fs.opendir(__dirname, (err, dir) => {
    assert.ifError(err);
    dir.closeSync();
  });
}

// fs.readdir
{
  assert.throws(() => {
    fs.readdir(blockedFolder, () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFolder),
  }));

  // doesNotThrow
  fs.readdir(__dirname, (err) => {
    assert.ifError(err);
  });
}

// fs.watch
{
  assert.throws(() => {
    fs.watch(blockedFile, () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFile),
  }));
  assert.throws(() => {
    fs.watch(relativeProtectedFile, () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(absoluteProtectedFile),
  }));

  // doesNotThrow
  fs.readdir(__dirname, (err) => {
    assert.ifError(err);
  });
}

// fs.rename
{
  assert.throws(() => {
    fs.rename(blockedFile, 'newfile', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(blockedFile),
  }));
  assert.throws(() => {
    fs.rename(relativeProtectedFile, 'newfile', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(absoluteProtectedFile),
  }));
}
tmpdir.refresh();
