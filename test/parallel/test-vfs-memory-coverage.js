// Flags: --expose-internals
'use strict';

// Cover MemoryProvider edge cases that aren't reached by the standard
// public-API tests: numeric flags, symlink loops, dynamic content
// providers, and lazy-populated directories.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');
const fs = require('fs');

// Numeric open flags (mirrors fs.constants.O_*) must be normalised to
// their string equivalents.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'orig');

  // O_RDONLY (0)
  let fd = myVfs.openSync('/file.txt', fs.constants.O_RDONLY);
  myVfs.closeSync(fd);

  // O_RDWR
  fd = myVfs.openSync('/file.txt', fs.constants.O_RDWR);
  myVfs.closeSync(fd);

  // O_WRONLY | O_CREAT | O_TRUNC = 'w'
  fd = myVfs.openSync('/created.txt',
                      fs.constants.O_WRONLY | fs.constants.O_CREAT | fs.constants.O_TRUNC);
  myVfs.closeSync(fd);

  // O_WRONLY | O_CREAT | O_EXCL = 'wx'
  fd = myVfs.openSync('/excl.txt',
                      fs.constants.O_WRONLY | fs.constants.O_CREAT | fs.constants.O_EXCL);
  myVfs.closeSync(fd);

  // 'wx' on existing file throws EEXIST
  assert.throws(
    () => myVfs.openSync('/file.txt',
                         fs.constants.O_WRONLY | fs.constants.O_CREAT | fs.constants.O_EXCL),
    { code: 'EEXIST' });

  // O_APPEND | O_RDWR | O_CREAT
  fd = myVfs.openSync('/app.txt',
                      fs.constants.O_APPEND | fs.constants.O_RDWR | fs.constants.O_CREAT);
  myVfs.closeSync(fd);

  // O_APPEND | O_EXCL | O_RDWR | O_CREAT = 'ax+'
  fd = myVfs.openSync('/axplus.txt',
                      fs.constants.O_APPEND | fs.constants.O_EXCL |
                      fs.constants.O_RDWR | fs.constants.O_CREAT);
  myVfs.closeSync(fd);

  // Bogus non-string non-number: defaults to 'r'
  fd = myVfs.openSync('/file.txt', null);
  myVfs.closeSync(fd);
}

// utimes with numeric (seconds) and Date arguments
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/u.txt', 'x');
  // numeric seconds branch
  myVfs.utimesSync('/u.txt', 1000, 2000);
  // Date branch
  myVfs.utimesSync('/u.txt', new Date(3000000), new Date(4000000));
}

// Symlink loop detection — covers createELOOP path
{
  const myVfs = vfs.create();
  myVfs.symlinkSync('/b', '/a');
  myVfs.symlinkSync('/a', '/b');
  assert.throws(() => myVfs.statSync('/a'), { code: 'ELOOP' });
  assert.throws(() => myVfs.realpathSync('/a'), { code: 'ELOOP' });
}

// Geometric buffer growth in writeSync — append many times to exercise
// the doubling path.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/grow.txt', 'x'.repeat(10));
  for (let i = 0; i < 8; i++) {
    myVfs.appendFileSync('/grow.txt', 'y'.repeat(2 ** i));
  }
  assert.ok(myVfs.readFileSync('/grow.txt').length > 250);
}

// Dynamic content providers (sync) and lazy directory population.
// These features have no public construction API, so we drive them
// directly through MemoryEntry / MemoryProvider internals.
{
  const memMod = require('internal/vfs/providers/memory');
  const { MemoryProvider } = memMod;
  const provider = new MemoryProvider();

  // Lazy-populated directory: install a populate callback on an existing
  // directory entry via the internal kRoot symbol.
  const symbols = Object.getOwnPropertySymbols(provider);
  const kRoot = symbols.find((s) => s.description === 'kRoot');
  assert.ok(kRoot, 'kRoot symbol expected on MemoryProvider');
  const root = provider[kRoot];

  // Manually create a lazy directory entry.
  const memEntryProto = Object.getPrototypeOf(root);
  const dir = Object.create(memEntryProto);
  dir.type = 1; // TYPE_DIR
  dir.mode = 0o755;
  dir.children = null;
  dir.populate = (scoped) => {
    scoped.addFile('hello.txt', 'lazy hello');
    scoped.addFile('dyn.txt', () => 'dynamic-string');
    scoped.addDirectory('subdir', null);
    scoped.addSymlink('link.txt', '/lazy/hello.txt');
  };
  dir.populated = false;
  dir.nlink = 1;
  dir.uid = 0;
  dir.gid = 0;
  const t = Date.now();
  dir.atime = t;
  dir.mtime = t;
  dir.ctime = t;
  dir.birthtime = t;
  // Borrow methods from an existing entry
  dir.isFile = root.isFile.bind(dir);
  dir.isDirectory = root.isDirectory.bind(dir);
  dir.isSymbolicLink = root.isSymbolicLink.bind(dir);
  dir.isDynamic = root.isDynamic.bind(dir);
  dir.getContentSync = root.getContentSync.bind(dir);
  dir.getContentAsync = root.getContentAsync.bind(dir);

  // Need a SafeMap children for the directory to behave correctly.
  dir.children = new Map();
  root.children.set('lazy', dir);

  const myVfs = vfs.create(provider);

  // Reading the lazy directory triggers populate
  const entries = myVfs.readdirSync('/lazy');
  assert.deepStrictEqual(entries.sort(), ['dyn.txt', 'hello.txt', 'link.txt', 'subdir']);

  // Static lazy file content
  assert.strictEqual(myVfs.readFileSync('/lazy/hello.txt', 'utf8'), 'lazy hello');

  // Dynamic content provider (sync, returns string)
  assert.strictEqual(myVfs.readFileSync('/lazy/dyn.txt', 'utf8'), 'dynamic-string');

  // Dynamic content provider (async via promises)
  myVfs.promises.readFile('/lazy/dyn.txt', 'utf8').then(common.mustCall((s) => {
    assert.strictEqual(s, 'dynamic-string');
  }));
}

// readdir basic — withFileTypes false returns names
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d');
  myVfs.writeFileSync('/d/a.txt', '');
  const names = myVfs.readdirSync('/d');
  assert.deepStrictEqual(names, ['a.txt']);
}

// rename onto an existing file overwrites
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'a');
  myVfs.writeFileSync('/b.txt', 'b');
  myVfs.renameSync('/a.txt', '/b.txt');
  assert.strictEqual(myVfs.readFileSync('/b.txt', 'utf8'), 'a');
  assert.strictEqual(myVfs.existsSync('/a.txt'), false);
}

// rmdir on non-empty directory throws ENOTEMPTY
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d');
  myVfs.writeFileSync('/d/x', '');
  assert.throws(() => myVfs.rmdirSync('/d'), { code: 'ENOTEMPTY' });
}

// chown / chmod / lutimes
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/p.txt', 'x');
  myVfs.symlinkSync('/p.txt', '/lk');
  myVfs.chmodSync('/p.txt', 0o600);
  myVfs.chownSync('/p.txt', 100, 200);
  myVfs.lutimesSync('/lk', new Date(0), new Date(0));
  const st = myVfs.statSync('/p.txt');
  assert.strictEqual(st.uid, 100);
  assert.strictEqual(st.gid, 200);
}

// utimes with string time (treated as DateNow)
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/u2.txt', 'x');
  myVfs.utimesSync('/u2.txt', 'now', 'now');
}

// readdir with mixed entry types (file, dir, symlink) — exercises
// non-recursive Dirent type branches.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d');
  myVfs.writeFileSync('/d/a.txt', 'x');
  myVfs.mkdirSync('/d/sub');
  myVfs.symlinkSync('a.txt', '/d/lnk');
  const dirents = myVfs.readdirSync('/d', { withFileTypes: true });
  const types = dirents.map((d) => d.name + ':' + (d.isFile() ? 'f' : d.isDirectory() ? 'd' : d.isSymbolicLink() ? 'l' : '?'));
  assert.ok(types.includes('a.txt:f'));
  assert.ok(types.includes('sub:d'));
  assert.ok(types.includes('lnk:l'));
}

// rename: type mismatches throw
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'x');
  myVfs.mkdirSync('/dir');
  // rename file onto dir → EISDIR
  assert.throws(() => myVfs.renameSync('/file.txt', '/dir'), { code: 'EISDIR' });
  // rename dir onto file → ENOTDIR
  assert.throws(() => myVfs.renameSync('/dir', '/file.txt'), { code: 'ENOTDIR' });
}

// link to a directory throws EINVAL
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d');
  assert.throws(() => myVfs.linkSync('/d', '/d-link'), { code: 'EINVAL' });
}

// link to existing target throws EEXIST
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'x');
  myVfs.writeFileSync('/b.txt', 'y');
  assert.throws(() => myVfs.linkSync('/a.txt', '/b.txt'), { code: 'EEXIST' });
}

// symlink with existing target throws EEXIST
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'x');
  assert.throws(() => myVfs.symlinkSync('/a.txt', '/a.txt'), { code: 'EEXIST' });
}

// readonly write paths throw EROFS
{
  const provider = new vfs.MemoryProvider();
  const myVfs = vfs.create(provider);
  myVfs.writeFileSync('/f.txt', 'x');
  myVfs.mkdirSync('/d');
  myVfs.symlinkSync('/f.txt', '/lnk');
  provider.setReadOnly();
  assert.throws(() => myVfs.openSync('/f.txt', 'w'), { code: 'EROFS' });
  assert.throws(() => myVfs.unlinkSync('/f.txt'), { code: 'EROFS' });
  assert.throws(() => myVfs.rmdirSync('/d'), { code: 'EROFS' });
  assert.throws(() => myVfs.renameSync('/f.txt', '/g.txt'), { code: 'EROFS' });
  assert.throws(() => myVfs.linkSync('/f.txt', '/h.txt'), { code: 'EROFS' });
  assert.throws(() => myVfs.symlinkSync('/x', '/y'), { code: 'EROFS' });
  assert.throws(() => myVfs.mkdirSync('/d2'), { code: 'EROFS' });
}

// open a file via a symlinked parent directory (covers the parent-symlink
// follow path in #ensureParent)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/real-dir');
  myVfs.writeFileSync('/real-dir/file.txt', 'hello');
  myVfs.symlinkSync('/real-dir', '/link-dir');
  // Read through the symlinked directory
  assert.strictEqual(myVfs.readFileSync('/link-dir/file.txt', 'utf8'), 'hello');
  // Write through the symlinked directory
  myVfs.writeFileSync('/link-dir/new.txt', 'new');
  assert.strictEqual(myVfs.readFileSync('/real-dir/new.txt', 'utf8'), 'new');
}

// ENOTDIR mid-path: writing through a non-directory parent fails ENOTDIR
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'x');
  // ensureParent walks the path and hits a file in the middle → ENOTDIR
  assert.throws(() => myVfs.writeFileSync('/file.txt/oops', 'y'),
                { code: 'ENOTDIR' });
}

// Dynamic content provider returning a Promise — sync API throws
{
  const memMod = require('internal/vfs/providers/memory');
  const { MemoryProvider } = memMod;
  const provider = new MemoryProvider();
  const symbols = Object.getOwnPropertySymbols(provider);
  const kRoot = symbols.find((s) => s.description === 'kRoot');
  const root = provider[kRoot];

  // Create a file with an async content provider
  const memEntryProto = Object.getPrototypeOf(root);
  const fileEntry = Object.create(memEntryProto);
  fileEntry.type = 0; // TYPE_FILE
  fileEntry.mode = 0o644;
  fileEntry.content = Buffer.alloc(0);
  fileEntry.contentProvider = async () => 'async-only';
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

  root.children.set('async-only.txt', fileEntry);

  const myVfs = vfs.create(provider);
  // Sync read with async provider throws ERR_INVALID_STATE
  assert.throws(() => myVfs.readFileSync('/async-only.txt'),
                { code: 'ERR_INVALID_STATE' });
  // Async read works
  myVfs.promises.readFile('/async-only.txt', 'utf8').then(common.mustCall((s) => {
    assert.strictEqual(s, 'async-only');
  }));
}

// MemoryProvider basic watch + watchAsync + watchFile
{
  const provider = new vfs.MemoryProvider();
  assert.strictEqual(provider.supportsWatch, true);
  const myVfs = vfs.create(provider);
  myVfs.writeFileSync('/wf.txt', 'a');

  const w = myVfs.watch('/wf.txt');
  w.close();

  const ai = myVfs.promises.watch('/wf.txt');
  ai.return().then(common.mustCall());

  const listener = () => {};
  myVfs.watchFile('/wf.txt', { interval: 1000, persistent: false }, listener);
  myVfs.unwatchFile('/wf.txt', listener);
}
