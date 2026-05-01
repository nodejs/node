'use strict';

// Cover small uncovered branches across the VFS subsystem.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// vfs.create with first arg as options (not a provider, no openSync method)
{
  const myVfs = vfs.create({ emitExperimentalWarning: false });
  assert.ok(myVfs);
  assert.ok(myVfs.provider instanceof vfs.MemoryProvider);
}

// new VirtualFileSystem(options) directly
{
  const myVfs = new vfs.VirtualFileSystem({ emitExperimentalWarning: false });
  assert.ok(myVfs);
  // emitExperimentalWarning option is validated as boolean
  assert.throws(() =>
    new vfs.VirtualFileSystem({ emitExperimentalWarning: 'not-bool' }),
  { code: 'ERR_INVALID_ARG_TYPE' });
}

// existsSync swallows path errors and returns false
{
  const myVfs = vfs.create();
  assert.strictEqual(myVfs.existsSync('/nope'), false);
}

// readdir({ withFileTypes: true, recursive: true }) — covers the recursive
// dirent path that fixes parentPath when names contain slashes.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/r/a/b', { recursive: true });
  myVfs.writeFileSync('/r/top.txt', 'x');
  myVfs.writeFileSync('/r/a/b/leaf.txt', 'y');

  const dirents = myVfs.readdirSync('/r', { withFileTypes: true, recursive: true });
  // Find the leaf in the recursive listing
  const leaf = dirents.find((d) => d.name === 'leaf.txt');
  assert.ok(leaf, 'leaf entry expected');
  assert.strictEqual(leaf.parentPath, '/r/a/b');

  // Top-level entry has parentPath = root
  const top = dirents.find((d) => d.name === 'top.txt');
  assert.ok(top);
  assert.strictEqual(top.parentPath, '/r');
}

// stats bigint paths for directories, symlinks, and zero-stats
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');
  myVfs.symlinkSync('/dir', '/link');
  myVfs.writeFileSync('/file.txt', 'x');

  const dirStat = myVfs.statSync('/dir', { bigint: true });
  assert.strictEqual(typeof dirStat.size, 'bigint');
  assert.strictEqual(dirStat.isDirectory(), true);

  const linkStat = myVfs.lstatSync('/link', { bigint: true });
  assert.strictEqual(typeof linkStat.size, 'bigint');
  assert.strictEqual(linkStat.isSymbolicLink(), true);
}

// watchFile on a missing file should emit zero-stats (covers createZeroStats).
// The initial poll establishes prev as zero-stats; once the file is created,
// the listener sees prev with size 0n.
{
  const myVfs = vfs.create();
  const watcher = myVfs.watchFile('/missing.txt',
                                  { interval: 50, persistent: false, bigint: true },
                                  common.mustCallAtLeast((curr, prev) => {
                                    assert.strictEqual(typeof curr.size, 'bigint');
                                    assert.strictEqual(typeof prev.size, 'bigint');
                                    myVfs.unwatchFile('/missing.txt');
                                  }, 1));
  setTimeout(() => myVfs.writeFileSync('/missing.txt', 'now-here'), 80);
  setTimeout(() => myVfs.unwatchFile('/missing.txt'), 500);
  if (watcher && watcher.unref) watcher.unref();
}

// VirtualDir read callback error path: pre-closed dir
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d');
  const dir = myVfs.opendirSync('/d');
  dir.closeSync();
  dir.read(common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_DIR_CLOSED');
  }));
  // entries() iterator on a closed dir throws when iterated
  (async () => {
    await assert.rejects(
      (async () => { for await (const _ of dir.entries()) {} })(), // eslint-disable-line no-unused-vars
      { code: 'ERR_DIR_CLOSED' },
    );
  })().then(common.mustCall());
  // [Symbol.dispose] is a no-op on an already-closed dir (must not throw)
  dir[Symbol.dispose]();
}

// async dir.close() returns a promise when invoked without a callback
(async () => {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d2');
  const dir = myVfs.opendirSync('/d2');
  await dir.close();
})().then(common.mustCall());

// createReadStream path getter coverage already in streams-misc; here we
// destroy the stream early to cover _destroy + _close paths.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/x.txt', 'data');
  const rs = myVfs.createReadStream('/x.txt');
  rs.on('error', () => {});
  rs.destroy();
}

// MemoryProvider setReadOnly — once read-only, writes throw EROFS
{
  const provider = new vfs.MemoryProvider();
  const myVfs = vfs.create(provider);
  myVfs.writeFileSync('/a.txt', 'x');
  provider.setReadOnly();
  assert.strictEqual(provider.readonly, true);
  assert.throws(() => myVfs.writeFileSync('/a.txt', 'y'), { code: 'EROFS' });
}
