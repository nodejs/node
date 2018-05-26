'use strict';

const common = require('../common');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const path = require('path');
const fs = require('fs');
const fsPromises = fs.promises;
const {
  access,
  chmod,
  copyFile,
  link,
  lchmod,
  lstat,
  mkdir,
  mkdtemp,
  open,
  readFile,
  readdir,
  readlink,
  realpath,
  rename,
  rmdir,
  stat,
  symlink,
  truncate,
  unlink,
  utimes
} = fsPromises;

const tmpDir = tmpdir.path;

common.crashOnUnhandledRejection();

// fs.promises should not be enumerable as long as it causes a warning to be
// emitted.
assert.strictEqual(Object.keys(fs).includes('promises'), false);

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

    let stats = await handle.stat();
    verifyStatObject(stats);
    assert.strictEqual(stats.size, 35);

    await handle.truncate(1);

    stats = await handle.stat();
    verifyStatObject(stats);
    assert.strictEqual(stats.size, 1);

    stats = await stat(dest);
    verifyStatObject(stats);

    stats = await handle.stat();
    verifyStatObject(stats);

    await handle.datasync();
    await handle.sync();

    const buf = Buffer.from('hello fsPromises');
    const bufLen = buf.length;
    await handle.write(buf);
    const ret = await handle.read(Buffer.alloc(bufLen), 0, bufLen, 0);
    assert.strictEqual(ret.bytesRead, bufLen);
    assert.deepStrictEqual(ret.buffer, buf);

    const buf2 = Buffer.from('hello FileHandle');
    const buf2Len = buf2.length;
    await handle.write(buf2, 0, buf2Len, 0);
    const ret2 = await handle.read(Buffer.alloc(buf2Len), 0, buf2Len, 0);
    assert.strictEqual(ret2.bytesRead, buf2Len);
    assert.deepStrictEqual(ret2.buffer, buf2);
    await truncate(dest, 5);
    assert.deepStrictEqual((await readFile(dest)).toString(), 'hello');

    await chmod(dest, 0o666);
    await handle.chmod(0o666);

    await chmod(dest, (0o10777));
    await handle.chmod(0o10777);

    await utimes(dest, new Date(), new Date());

    try {
      await handle.utimes(new Date(), new Date());
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

    if (common.canCreateSymLink()) {
      const newLink = path.resolve(tmpDir, 'baz3.js');
      await symlink(newPath, newLink);
      stats = await lstat(newLink);
      verifyStatObject(stats);

      assert.strictEqual(newPath.toLowerCase(),
                         (await realpath(newLink)).toLowerCase());
      assert.strictEqual(newPath.toLowerCase(),
                         (await readlink(newLink)).toLowerCase());
      if (common.isOSX) {
        // lchmod is only available on macOS
        const newMode = 0o666;
        await lchmod(newLink, newMode);
        stats = await lstat(newLink);
        assert.strictEqual(stats.mode & 0o777, newMode);
      }


      await unlink(newLink);
    }

    const newLink2 = path.resolve(tmpDir, 'baz4.js');
    await link(newPath, newLink2);

    await unlink(newLink2);

    const newdir = path.resolve(tmpDir, 'dir');
    await mkdir(newdir);
    stats = await stat(newdir);
    assert(stats.isDirectory());

    const list = await readdir(tmpDir);
    assert.deepStrictEqual(list, ['baz2.js', 'dir']);

    await rmdir(newdir);

    await mkdtemp(path.resolve(tmpDir, 'FOO'));
    assert.rejects(
      // mkdtemp() expects to get a string prefix.
      async () => mkdtemp(1),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError [ERR_INVALID_ARG_TYPE]'
      }
    );

  }

  doTest().then(common.mustCall());
}
