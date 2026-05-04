// Flags: --experimental-vfs
'use strict';

// Symlink and path-escape behaviour for RealFSProvider.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

tmpdir.refresh();
const root = path.join(tmpdir.path, 'real-symlinks');
fs.mkdirSync(root, { recursive: true });
const myVfs = vfs.create(new vfs.RealFSProvider(root));

(async () => {
  // .. traversal in VFS paths can't escape root
  {
    const subDir = path.join(root, 'sandbox');
    fs.mkdirSync(subDir, { recursive: true });
    const subVfs = vfs.create(new vfs.RealFSProvider(subDir));
    assert.throws(() => subVfs.statSync('/../hello.txt'), { code: 'ENOENT' });
    assert.throws(() => subVfs.readFileSync('/../../../etc/passwd'),
                  { code: 'ENOENT' });
    fs.rmdirSync(subDir);
  }

  // Path traversal via a non-leading-slash relative path
  {
    const escapeProvider = new vfs.RealFSProvider(root);
    assert.throws(() => escapeProvider.statSync('../etc/passwd'),
                  { code: 'ENOENT' });
  }

  // Symlinks: absolute target rejected with EACCES
  {
    assert.throws(() => myVfs.symlinkSync('/etc/passwd', '/escape'),
                  { code: 'EACCES' });
    await assert.rejects(myVfs.promises.symlink('/etc/passwd', '/escape2'),
                         { code: 'EACCES' });
  }

  // Symlinks: relative target outside root rejected with EACCES
  {
    assert.throws(() => myVfs.symlinkSync('../../escape', '/bad-link'),
                  { code: 'EACCES' });
    await assert.rejects(
      myVfs.promises.symlink('../../escape', '/bad-link2'),
      { code: 'EACCES' });
  }

  // Symlink with relative target inside root — readlink returns target as-is
  {
    fs.writeFileSync(path.join(root, 'rel-target.txt'), 'ok');
    fs.symlinkSync('rel-target.txt', path.join(root, 'rel-link'));
    assert.strictEqual(myVfs.readlinkSync('/rel-link'), 'rel-target.txt');
    assert.strictEqual(await myVfs.promises.readlink('/rel-link'),
                       'rel-target.txt');
  }

  // Symlink whose absolute target is inside root → translated to VFS path
  {
    fs.writeFileSync(path.join(root, 'target.txt'), 'x');
    fs.symlinkSync(path.join(root, 'target.txt'),
                   path.join(root, 'abs-link'));
    assert.strictEqual(myVfs.readlinkSync('/abs-link'), '/target.txt');
    assert.strictEqual(await myVfs.promises.readlink('/abs-link'),
                       '/target.txt');
  }

  // Symlink whose absolute target equals root → '/'
  {
    fs.symlinkSync(root, path.join(root, 'root-link'));
    assert.strictEqual(myVfs.readlinkSync('/root-link'), '/');
    assert.strictEqual(await myVfs.promises.readlink('/root-link'), '/');
  }

  // Symlink that points outside root: realpath rejects with EACCES
  {
    fs.writeFileSync(path.join(tmpdir.path, 'outside.txt'), 'forbidden');
    fs.symlinkSync(path.join(tmpdir.path, 'outside.txt'),
                   path.join(root, 'esc-link'));

    // Readlink returns the absolute target as-is (no translation since it's
    // outside the root)
    const target = myVfs.readlinkSync('/esc-link');
    assert.strictEqual(target, path.join(tmpdir.path, 'outside.txt'));

    // Realpath rejects: the resolved target escapes root
    assert.throws(() => myVfs.realpathSync('/esc-link'), { code: 'EACCES' });
    await assert.rejects(myVfs.promises.realpath('/esc-link'),
                         { code: 'EACCES' });
  }

  // Realpath on root and on a subdir
  {
    fs.mkdirSync(path.join(root, 'sub2'), { recursive: true });
    assert.strictEqual(myVfs.realpathSync('/'), '/');
    assert.strictEqual(myVfs.realpathSync('/sub2'), '/sub2');
    assert.strictEqual(await myVfs.promises.realpath('/sub2'), '/sub2');
  }

  // RealFSProvider with a rootPath that already ends in path.sep
  {
    fs.writeFileSync(path.join(root, 'tr.txt'), 'tr');
    const trailingProvider = new vfs.RealFSProvider(root + path.sep);
    assert.strictEqual(trailingProvider.readFileSync('/tr.txt', 'utf8'), 'tr');
  }
})().then(common.mustCall());
