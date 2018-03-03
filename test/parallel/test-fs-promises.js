'use strict';

const common = require('../common');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const path = require('path');
const fsPromises = require('fs/promises');
const {
  access,
  chmod,
  copyFile,
  fchmod,
  fdatasync,
  fstat,
  fsync,
  ftruncate,
  futimes,
  link,
  lstat,
  mkdir,
  mkdtemp,
  open,
  read,
  readdir,
  readlink,
  realpath,
  rename,
  rmdir,
  stat,
  symlink,
  write,
  unlink,
  utimes
} = fsPromises;

const tmpDir = tmpdir.path;

common.crashOnUnhandledRejection();

{
  access(__filename, 'r')
    .then(common.mustCall())
    .catch(common.mustNotCall());

  access('this file does not exist', 'r')
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ENOENT',
      type: Error,
      message:
        /^ENOENT: no such file or directory, access/
    }));
}

function verifyStatObject(stat) {
  assert.strictEqual(typeof stat, 'object');
  assert.strictEqual(typeof stat.dev, 'number');
  assert.strictEqual(typeof stat.mode, 'number');
}

{
  async function doTest() {
    tmpdir.refresh();
    const dest = path.resolve(tmpDir, 'baz.js');
    await copyFile(fixtures.path('baz.js'), dest);
    await access(dest, 'r');

    const handle = await open(dest, 'r+');
    assert.strictEqual(typeof handle, 'object');

    let stats = await fstat(handle);
    verifyStatObject(stats);
    assert.strictEqual(stats.size, 35);

    await ftruncate(handle, 1);

    stats = await fstat(handle);
    verifyStatObject(stats);
    assert.strictEqual(stats.size, 1);

    stats = await stat(dest);
    verifyStatObject(stats);

    await fdatasync(handle);
    await fsync(handle);

    const buf = Buffer.from('hello world');

    await write(handle, buf);

    const ret = await read(handle, Buffer.alloc(11), 0, 11, 0);
    assert.strictEqual(ret.bytesRead, 11);
    assert.deepStrictEqual(ret.buffer, buf);

    await chmod(dest, 0o666);
    await fchmod(handle, 0o666);

    await utimes(dest, new Date(), new Date());

    try {
      await futimes(handle, new Date(), new Date());
    } catch (err) {
      // Some systems do not have futimes. If there is an error,
      // expect it to be ENOSYS
      common.expectsError({
        code: 'ENOSYS',
        type: Error
      })(err);
    }

    await handle.close();

    const newPath = path.resolve(tmpDir, 'baz2.js');
    await rename(dest, newPath);
    stats = await stat(newPath);
    verifyStatObject(stats);

    const newLink = path.resolve(tmpDir, 'baz3.js');
    await symlink(newPath, newLink);

    const newLink2 = path.resolve(tmpDir, 'baz4.js');
    await link(newPath, newLink2);

    stats = await lstat(newLink);
    verifyStatObject(stats);

    assert.strictEqual(newPath.toLowerCase(),
                       (await realpath(newLink)).toLowerCase());
    assert.strictEqual(newPath.toLowerCase(),
                       (await readlink(newLink)).toLowerCase());

    await unlink(newLink);
    await unlink(newLink2);

    const newdir = path.resolve(tmpDir, 'dir');
    await mkdir(newdir);
    stats = await stat(newdir);
    assert(stats.isDirectory());

    const list = await readdir(tmpDir);
    assert.deepStrictEqual(list, ['baz2.js', 'dir']);

    await rmdir(newdir);

    await mkdtemp(path.resolve(tmpDir, 'FOO'));
  }

  doTest().then(common.mustCall());
}
