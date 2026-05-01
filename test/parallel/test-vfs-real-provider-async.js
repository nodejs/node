'use strict';

// Cover RealFSProvider async methods, symlinks, watch, and edge cases.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

tmpdir.refresh();
const root = path.join(tmpdir.path, 'real-provider-async');
fs.mkdirSync(root, { recursive: true });

const myVfs = vfs.create(new vfs.RealFSProvider(root));

(async () => {
  // writeFile + readFile (async)
  await myVfs.promises.writeFile('/a.txt', 'hello');
  assert.strictEqual(await myVfs.promises.readFile('/a.txt', 'utf8'), 'hello');

  // stat / lstat / access async
  const st = await myVfs.promises.stat('/a.txt');
  assert.strictEqual(st.size, 5);
  await myVfs.promises.access('/a.txt');

  // mkdir / readdir / rmdir async
  await myVfs.promises.mkdir('/d/sub', { recursive: true });
  const entries = await myVfs.promises.readdir('/d');
  assert.deepStrictEqual(entries.sort(), ['sub']);
  await myVfs.promises.rmdir('/d/sub');

  // rename async
  await myVfs.promises.writeFile('/old.txt', 'x');
  await myVfs.promises.rename('/old.txt', '/new.txt');
  assert.strictEqual(myVfs.existsSync('/old.txt'), false);
  assert.strictEqual(myVfs.existsSync('/new.txt'), true);

  // unlink async
  await myVfs.promises.unlink('/new.txt');
  assert.strictEqual(myVfs.existsSync('/new.txt'), false);

  // copyFile async
  await myVfs.promises.copyFile('/a.txt', '/copy.txt');
  assert.strictEqual(await myVfs.promises.readFile('/copy.txt', 'utf8'), 'hello');

  // realpath / readlink async (with relative target staying in root)
  await myVfs.promises.symlink('a.txt', '/link');
  assert.strictEqual(await myVfs.promises.readlink('/link'), 'a.txt');
  assert.strictEqual(await myVfs.promises.realpath('/link'), '/a.txt');
  // realpath on root
  assert.strictEqual(myVfs.realpathSync('/'), '/');
})().then(common.mustCall());

// Symlinks: absolute target rejected with EACCES
{
  assert.throws(
    () => myVfs.symlinkSync('/etc/passwd', '/escape'),
    { code: 'EACCES' },
  );
}

// promises.symlink with absolute target also rejected
(async () => {
  await assert.rejects(
    myVfs.promises.symlink('/etc/passwd', '/escape2'),
    { code: 'EACCES' },
  );
})().then(common.mustCall());

// readlinkSync on a symlink whose target is inside root → translated to VFS '/'-rooted path
{
  // First put a file at root
  fs.writeFileSync(path.join(root, 'target.txt'), 'x');
  // Make a symlink whose absolute target is inside root via real fs
  fs.symlinkSync(path.join(root, 'target.txt'), path.join(root, 'abs-link'));
  const target = myVfs.readlinkSync('/abs-link');
  // Should translate to '/target.txt' (VFS-relative)
  assert.strictEqual(target, '/target.txt');
}

// readlinkSync where target == root → '/'
{
  fs.symlinkSync(root, path.join(root, 'root-link'));
  assert.strictEqual(myVfs.readlinkSync('/root-link'), '/');
  myVfs.promises.readlink('/root-link').then(common.mustCall(
    (t) => assert.strictEqual(t, '/'),
  ));
}

// realpathSync on root subdir
{
  fs.mkdirSync(path.join(root, 'sub2'), { recursive: true });
  assert.strictEqual(myVfs.realpathSync('/sub2'), '/sub2');
  myVfs.promises.realpath('/sub2').then(common.mustCall(
    (p) => assert.strictEqual(p, '/sub2'),
  ));
}

// Watch capability and method calls (real fs)
{
  assert.strictEqual(myVfs.provider.supportsWatch, true);
  fs.writeFileSync(path.join(root, 'watch-me.txt'), 'a');

  const watcher = myVfs.watch('/watch-me.txt', { persistent: false });
  watcher.close();
}

// promises.watch returns an async iterable (we just call .return() to close it)
(async () => {
  fs.writeFileSync(path.join(root, 'pwatch.txt'), 'a');
  const iter = myVfs.promises.watch('/pwatch.txt', { persistent: false });
  await iter.return();
})().then(common.mustCall());

// watchFile / unwatchFile
{
  fs.writeFileSync(path.join(root, 'wf.txt'), 'a');
  const listener = () => {};
  myVfs.watchFile('/wf.txt', { persistent: false }, listener);
  myVfs.unwatchFile('/wf.txt', listener);
}
