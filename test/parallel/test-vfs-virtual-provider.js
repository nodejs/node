// Flags: --experimental-vfs
'use strict';

// Exercise the VirtualProvider base class — its capability flags,
// readonly stubs, and the default implementations built on primitives.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Bare base provider: every primitive throws ERR_METHOD_NOT_IMPLEMENTED.
{
  const p = new vfs.VirtualProvider();
  assert.strictEqual(p.readonly, false);
  assert.strictEqual(p.supportsSymlinks, false);
  assert.strictEqual(p.supportsWatch, false);

  for (const method of ['openSync', 'statSync', 'readdirSync', 'mkdirSync',
                        'rmdirSync', 'unlinkSync', 'renameSync',
                        'linkSync', 'readlinkSync', 'symlinkSync',
                        'watch', 'watchAsync', 'watchFile', 'unwatchFile']) {
    assert.throws(() => p[method]('/x', '/y'),
                  { code: 'ERR_METHOD_NOT_IMPLEMENTED' },
                  `${method} must throw`);
  }

  // Async primitives reject with the same error
  for (const method of ['open', 'stat', 'readdir', 'mkdir', 'rmdir', 'unlink',
                        'rename', 'link', 'readlink', 'symlink']) {
    assert.rejects(p[method]('/x', '/y'),
                   { code: 'ERR_METHOD_NOT_IMPLEMENTED' },
                   `${method} must reject`).then(common.mustCall());
  }

  // lstat/lstatSync default to stat — should propagate the not-impl error
  assert.throws(() => p.lstatSync('/x'),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
  assert.rejects(p.lstat('/x'),
                 { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());

  // existsSync / exists default impl: stat throws → false
  assert.strictEqual(p.existsSync('/x'), false);
  p.exists('/x').then(common.mustCall((r) => {
    assert.strictEqual(r, false);
  }));
}

// Read-only provider: write primitives throw EROFS instead of NOT_IMPL.
{
  const p = new class extends vfs.VirtualProvider {
    get readonly() { return true; }
  }();
  for (const method of ['mkdirSync', 'rmdirSync', 'unlinkSync',
                        'renameSync', 'linkSync', 'symlinkSync']) {
    assert.throws(() => p[method]('/x', '/y'),
                  { code: 'EROFS' },
                  `${method} must throw EROFS`);
  }
  for (const method of ['mkdir', 'rmdir', 'unlink', 'rename', 'link', 'symlink']) {
    assert.rejects(p[method]('/x', '/y'),
                   { code: 'EROFS' }).then(common.mustCall());
  }

  // copyFile / writeFile / appendFile reject with EROFS
  assert.rejects(p.copyFile('/a', '/b'),
                 { code: 'EROFS' }).then(common.mustCall());
  assert.throws(() => p.copyFileSync('/a', '/b'),
                { code: 'EROFS' });
  assert.rejects(p.writeFile('/a', 'x'),
                 { code: 'EROFS' }).then(common.mustCall());
  assert.throws(() => p.writeFileSync('/a', 'x'),
                { code: 'EROFS' });
  assert.rejects(p.appendFile('/a', 'x'),
                 { code: 'EROFS' }).then(common.mustCall());
  assert.throws(() => p.appendFileSync('/a', 'x'),
                { code: 'EROFS' });
}

// Default access / realpath / copyFile delegate to stat + read/write
{
  // Use MemoryProvider with the public API to verify delegation paths,
  // since the base class needs working primitives.
  const myVfs = vfs.create();
  myVfs.writeFileSync('/src.txt', 'hello');

  // Realpath default: returns path as-is after stat — covered by myVfs.realpathSync
  assert.strictEqual(myVfs.realpathSync('/src.txt'), '/src.txt');

  // exists default impl
  assert.strictEqual(myVfs.provider.existsSync('src.txt'), true);
  assert.strictEqual(myVfs.provider.existsSync('missing.txt'), false);

  // copyFile via base class default path (MemoryProvider doesn't override)
  myVfs.provider.copyFileSync('src.txt', 'dst.txt');
  assert.strictEqual(myVfs.readFileSync('/dst.txt', 'utf8'), 'hello');

  // copyFile with COPYFILE_EXCL and existing dest must throw
  const COPYFILE_EXCL = 1;
  assert.throws(() =>
    myVfs.provider.copyFileSync('src.txt', 'dst.txt', COPYFILE_EXCL),
                { code: 'EEXIST' });

  // accessSync with mode=0 (existence-only)
  myVfs.provider.accessSync('src.txt', 0);

  // accessSync R_OK on a readable file should pass
  const R_OK = 4;
  myVfs.provider.accessSync('src.txt', R_OK);
}
