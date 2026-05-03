// Flags: --expose-internals
'use strict';

// Cover branch paths in MemoryProvider that aren't reached by the
// happy-path tests.

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// utimes with non-number / non-string / non-object → falls through to
// `return time;` in toMs.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/u.txt', 'x');
  myVfs.utimesSync('/u.txt', null, undefined);
}

// utimes with object Date instances
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/u2.txt', 'x');
  myVfs.utimesSync('/u2.txt', new Date(0), new Date(1));
}

// normalizeFlags: every numeric flag combination
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/seed.txt', 'x');
  // 'a' = append + write only (no rdwr)
  myVfs.openSync('/append.txt',
                 fs.constants.O_APPEND | fs.constants.O_CREAT |
                 fs.constants.O_WRONLY).closeSync;
  // 'a+' = append + rdwr
  myVfs.openSync('/append-plus.txt',
                 fs.constants.O_APPEND | fs.constants.O_CREAT |
                 fs.constants.O_RDWR).closeSync;
  // 'ax' = append + excl
  myVfs.openSync('/ax.txt',
                 fs.constants.O_APPEND | fs.constants.O_EXCL |
                 fs.constants.O_CREAT | fs.constants.O_WRONLY).closeSync;
  // 'r+' = rdwr (no write/append/excl)
  myVfs.openSync('/seed.txt', fs.constants.O_RDWR).closeSync;
}

// mkdir recursive: an intermediate path is a regular file → ENOTDIR
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/blocker', 'x');
  assert.throws(
    () => myVfs.mkdirSync('/blocker/sub', { recursive: true }),
    { code: 'ENOTDIR' },
  );
}

// mkdir with explicit mode
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d-mode', { mode: 0o700 });
  const st = myVfs.statSync('/d-mode');
  assert.strictEqual(st.mode & 0o777, 0o700);

  myVfs.mkdirSync('/r-mode/sub/deep', { recursive: true, mode: 0o700 });
  assert.strictEqual(
    myVfs.statSync('/r-mode/sub/deep').mode & 0o777, 0o700);
}

// rename within the same parent
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d');
  myVfs.writeFileSync('/d/a.txt', 'x');
  myVfs.renameSync('/d/a.txt', '/d/b.txt');
  assert.strictEqual(myVfs.existsSync('/d/a.txt'), false);
  assert.strictEqual(myVfs.existsSync('/d/b.txt'), true);
}

// Dynamic content provider returning a Buffer (not string)
{
  const memMod = require('internal/vfs/providers/memory');
  const { MemoryProvider } = memMod;
  const provider = new MemoryProvider();
  const symbols = Object.getOwnPropertySymbols(provider);
  const kRoot = symbols.find((s) => s.description === 'kRoot');
  const root = provider[kRoot];
  const memEntryProto = Object.getPrototypeOf(root);

  const fileEntry = Object.create(memEntryProto);
  fileEntry.type = 0; // TYPE_FILE
  fileEntry.mode = 0o644;
  fileEntry.content = Buffer.alloc(0);
  fileEntry.contentProvider = () => Buffer.from('buffer-content');
  fileEntry.children = null;
  fileEntry.target = null;
  fileEntry.populate = null;
  fileEntry.populated = true;
  fileEntry.nlink = 1;
  fileEntry.uid = 0;
  fileEntry.gid = 0;
  const t = Date.now();
  fileEntry.atime = t;
  fileEntry.mtime = t;
  fileEntry.ctime = t;
  fileEntry.birthtime = t;
  fileEntry.isFile = root.isFile.bind(fileEntry);
  fileEntry.isDirectory = root.isDirectory.bind(fileEntry);
  fileEntry.isSymbolicLink = root.isSymbolicLink.bind(fileEntry);
  fileEntry.isDynamic = root.isDynamic.bind(fileEntry);
  fileEntry.getContentSync = root.getContentSync.bind(fileEntry);
  fileEntry.getContentAsync = root.getContentAsync.bind(fileEntry);
  root.children.set('buf-dyn.txt', fileEntry);

  const myVfs = vfs.create(provider);
  assert.strictEqual(myVfs.readFileSync('/buf-dyn.txt', 'utf8'), 'buffer-content');
}

// resolveSymlinkTarget with absolute target (root-relative) within VFS
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');
  myVfs.writeFileSync('/dir/file.txt', 'hi');
  myVfs.symlinkSync('/dir', '/abs-link');
  assert.strictEqual(myVfs.readFileSync('/abs-link/file.txt', 'utf8'), 'hi');
}

// Lookup root path (resolves to root via early return)
{
  const myVfs = vfs.create();
  const st = myVfs.statSync('/');
  assert.ok(st.isDirectory());
}

// Symlink loop in intermediate path (ELOOP)
{
  const myVfs = vfs.create();
  myVfs.symlinkSync('/loop2', '/loop1');
  myVfs.symlinkSync('/loop1', '/loop2');
  assert.throws(() => myVfs.statSync('/loop1/sub'),
                { code: 'ELOOP' });
}

// rename with same destination throws when types differ — exercises
// the existingDest type-mismatch checks already; here cover the
// successful overwrite of a file by a file.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/x.txt', 'old');
  myVfs.writeFileSync('/y.txt', 'new');
  myVfs.renameSync('/y.txt', '/x.txt');
  assert.strictEqual(myVfs.readFileSync('/x.txt', 'utf8'), 'new');
}
