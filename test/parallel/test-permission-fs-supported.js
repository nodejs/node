'use strict';

require('../common');
const assert = require('assert');

// Most of the times, the function called for async and Sync
// methods are the same on node_file.cc
function syncAndAsyncAPI(funcName) {
  return [funcName, funcName + 'Sync'];
}

// This tests guarantee whenever a new API under fs module is exposed
// it must contain a test to the permission model.
// Otherwise, a vulnerability might be exposed. If you are adding a new
// fs method, please, make sure to include a test for it on test-permission-fs-*
// and include to the supportedApis list.
//
//
// This list is synced with
//   fixtures/permission/fs-read and
//   fixtures/permission/fs-write
const supportedApis = [
  ...syncAndAsyncAPI('appendFile'),
  ...syncAndAsyncAPI('access'),
  ...syncAndAsyncAPI('chown'),
  ...syncAndAsyncAPI('chmod'),
  ...syncAndAsyncAPI('copyFile'),
  ...syncAndAsyncAPI('cp'),
  'createReadStream',
  'createWriteStream',
  ...syncAndAsyncAPI('exists'),
  ...syncAndAsyncAPI('lchown'),
  ...syncAndAsyncAPI('lchmod'),
  ...syncAndAsyncAPI('link'),
  ...syncAndAsyncAPI('lutimes'),
  ...syncAndAsyncAPI('mkdir'),
  ...syncAndAsyncAPI('mkdtemp'),
  ...syncAndAsyncAPI('open'),
  'openAsBlob',
  ...syncAndAsyncAPI('mkdtemp'),
  'mkdtempDisposableSync',
  ...syncAndAsyncAPI('readdir'),
  ...syncAndAsyncAPI('readFile'),
  ...syncAndAsyncAPI('readlink'),
  ...syncAndAsyncAPI('rename'),
  ...syncAndAsyncAPI('rm'),
  ...syncAndAsyncAPI('rmdir'),
  ...syncAndAsyncAPI('stat'),
  ...syncAndAsyncAPI('statfs'),
  ...syncAndAsyncAPI('statfs'),
  ...syncAndAsyncAPI('symlink'),
  ...syncAndAsyncAPI('truncate'),
  ...syncAndAsyncAPI('unlink'),
  ...syncAndAsyncAPI('utimes'),
  'watch',
  'watchFile',
  ...syncAndAsyncAPI('writeFile'),
  ...syncAndAsyncAPI('opendir'),
];

// Non functions
const ignoreList = [
  'constants',
  'promises',
  'X_OK',
  'W_OK',
  'R_OK',
  'F_OK',
  'Dir',
  // the Utf8Stream is implemented in terms of functions
  // on the fs module that have permission checks, so we don't
  // need to check it here.
  'Utf8Stream',
  'FileReadStream',
  'FileWriteStream',
  '_toUnixTimestamp',
  'Stats',
  'ReadStream',
  'WriteStream',
  'Dirent',
  // fs.watch is already blocked
  'unwatchFile',
  ...syncAndAsyncAPI('lstat'),
  ...syncAndAsyncAPI('realpath'),
  // File descriptor–based metadata operations
  //
  // The kernel does not allow opening a file descriptor for an inode
  // with write access if the inode itself is read-only. However, it still
  // permits modifying the inode’s metadata (e.g., permission bits, ownership,
  // timestamps) because you own the file. These changes can be made either
  // by referring to the file by name (e.g., chmod) or through any existing
  // file descriptor that identifies the same inode (e.g., fchmod).
  //
  // If the kernel required write access to change metadata, it would be
  // impossible to modify the permissions of a file once it was made read-only.
  // For that reason, syscalls such as fchmod, fchown, and futimes bypass
  // the file descriptor’s access mode. Even a read-only ('r') descriptor
  // can still update metadata. To prevent unintended modifications,
  // these APIs are therefore blocked by default when permission model is
  // enabled.
  ...syncAndAsyncAPI('close'),
  ...syncAndAsyncAPI('fchown'),
  ...syncAndAsyncAPI('fchmod'),
  ...syncAndAsyncAPI('fdatasync'),
  ...syncAndAsyncAPI('fstat'),
  ...syncAndAsyncAPI('fsync'),
  ...syncAndAsyncAPI('ftruncate'),
  ...syncAndAsyncAPI('futimes'),
  ...syncAndAsyncAPI('read'),
  ...syncAndAsyncAPI('readv'),
  ...syncAndAsyncAPI('write'),
  ...syncAndAsyncAPI('writev'),
  ...syncAndAsyncAPI('glob'),
];

{
  const fsList = Object.keys(require('fs'));
  for (const k of fsList) {
    if (!supportedApis.includes(k) && !ignoreList.includes(k)) {
      assert.fail(`fs.${k} was exposed but is neither on the supported list ` +
      'of the permission model nor on the ignore list.');
    }
  }
}
