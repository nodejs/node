'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');

// Check if the two constants accepted by chmod() on Windows are defined.
assert.notStrictEqual(fs.constants.S_IRUSR, undefined);
assert.notStrictEqual(fs.constants.S_IWUSR, undefined);

// O_SYNC, O_DSYNC and O_DIRECT have no POSIX macro on Windows, but libuv
// honors them via FILE_FLAG_WRITE_THROUGH / FILE_FLAG_NO_BUFFERING, so they
// are exposed under their portable names with libuv's flag values.
if (common.isWindows) {
  assert.strictEqual(fs.constants.O_SYNC, 0x08000000);
  assert.strictEqual(fs.constants.O_DSYNC, 0x04000000);
  assert.strictEqual(fs.constants.O_DIRECT, 0x02000000);
}

// Check null prototype.
assert.strictEqual(Object.getPrototypeOf(fs.constants), null);

const knownFsConstantNames = [
  'UV_FS_SYMLINK_DIR',
  'UV_FS_SYMLINK_JUNCTION',
  'O_RDONLY',
  'O_WRONLY',
  'O_RDWR',
  'UV_DIRENT_UNKNOWN',
  'UV_DIRENT_FILE',
  'UV_DIRENT_DIR',
  'UV_DIRENT_LINK',
  'UV_DIRENT_FIFO',
  'UV_DIRENT_SOCKET',
  'UV_DIRENT_CHAR',
  'UV_DIRENT_BLOCK',
  'S_IFMT',
  'S_IFREG',
  'S_IFDIR',
  'S_IFCHR',
  'S_IFBLK',
  'S_IFIFO',
  'S_IFLNK',
  'S_IFSOCK',
  'O_CREAT',
  'O_EXCL',
  'UV_FS_O_FILEMAP',
  'O_NOCTTY',
  'O_TRUNC',
  'O_APPEND',
  'O_DIRECTORY',
  'O_EXCL',
  'O_NOATIME',
  'O_NOFOLLOW',
  'O_SYNC',
  'O_DSYNC',
  'O_SYMLINK',
  'O_DIRECT',
  'O_NONBLOCK',
  'S_IRWXU',
  'S_IRUSR',
  'S_IWUSR',
  'S_IXUSR',
  'S_IRWXG',
  'S_IRGRP',
  'S_IWGRP',
  'S_IXGRP',
  'S_IRWXO',
  'S_IROTH',
  'S_IWOTH',
  'S_IXOTH',
  'F_OK',
  'R_OK',
  'W_OK',
  'X_OK',
  'UV_FS_COPYFILE_EXCL',
  'COPYFILE_EXCL',
  'UV_FS_COPYFILE_FICLONE',
  'COPYFILE_FICLONE',
  'UV_FS_COPYFILE_FICLONE_FORCE',
  'COPYFILE_FICLONE_FORCE',
];
const fsConstantNames = Object.keys(fs.constants);
const unknownFsConstantNames = fsConstantNames.filter((constant) => {
  return !knownFsConstantNames.includes(constant);
});
assert.deepStrictEqual(unknownFsConstantNames, [], `Unknown fs.constants: ${unknownFsConstantNames.join(', ')}`);
