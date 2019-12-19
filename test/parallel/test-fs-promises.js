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
  chown,
  copyFile,
  lchown,
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
  utimes,
  writeFile
} = fsPromises;

const tmpDir = tmpdir.path;

let dirc = 0;
function nextdir() {
  return `test${++dirc}`;
}

// fs.promises should be enumerable.
assert.strictEqual(
  Object.prototype.propertyIsEnumerable.call(fs, 'promises'),
  true
);

{
  access(__filename, 0)
    .then(common.mustCall());

  assert.rejects(
    access('this file does not exist', 0),
    {
      code: 'ENOENT',
      name: 'Error',
      message: /^ENOENT: no such file or directory, access/
    }
  );

  assert.rejects(
    access(__filename, 8),
    {
      code: 'ERR_OUT_OF_RANGE',
      message: /"mode".*must be an integer >= 0 && <= 7\. Received 8$/
    }
  );

  assert.rejects(
    access(__filename, { [Symbol.toPrimitive]() { return 5; } }),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /"mode" argument.+integer\. Received an instance of Object$/
    }
  );
}

function verifyStatObject(stat) {
  assert.strictEqual(typeof stat, 'object');
  assert.strictEqual(typeof stat.dev, 'number');
  assert.strictEqual(typeof stat.mode, 'number');
}

async function getHandle(dest) {
  await copyFile(fixtures.path('baz.js'), dest);
  await access(dest);

  return open(dest, 'r+');
}

{
  async function doTest() {
    tmpdir.refresh();

    const dest = path.resolve(tmpDir, 'baz.js');

    // handle is object
    {
      const handle = await getHandle(dest);
      assert.strictEqual(typeof handle, 'object');
      await handle.close();
    }

    // file stats
    {
      const handle = await getHandle(dest);
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
      await handle.close();
    }

    // Test fs.read promises when length to read is zero bytes
    {
      const dest = path.resolve(tmpDir, 'test1.js');
      const handle = await getHandle(dest);
      const buf = Buffer.from('DAWGS WIN');
      const bufLen = buf.length;
      await handle.write(buf);
      const ret = await handle.read(Buffer.alloc(bufLen), 0, 0, 0);
      assert.strictEqual(ret.bytesRead, 0);

      await unlink(dest);
      await handle.close();
    }

    // Bytes written to file match buffer
    {
      const handle = await getHandle(dest);
      const buf = Buffer.from('hello fsPromises');
      const bufLen = buf.length;
      await handle.write(buf);
      const ret = await handle.read(Buffer.alloc(bufLen), 0, bufLen, 0);
      assert.strictEqual(ret.bytesRead, bufLen);
      assert.deepStrictEqual(ret.buffer, buf);
      await handle.close();
    }

    // Truncate file to specified length
    {
      const handle = await getHandle(dest);
      const buf = Buffer.from('hello FileHandle');
      const bufLen = buf.length;
      await handle.write(buf, 0, bufLen, 0);
      const ret = await handle.read(Buffer.alloc(bufLen), 0, bufLen, 0);
      assert.strictEqual(ret.bytesRead, bufLen);
      assert.deepStrictEqual(ret.buffer, buf);
      await truncate(dest, 5);
      assert.deepStrictEqual((await readFile(dest)).toString(), 'hello');
      await handle.close();
    }

    // Invalid change of ownership
    {
      const handle = await getHandle(dest);

      await chmod(dest, 0o666);
      await handle.chmod(0o666);

      await chmod(dest, (0o10777));
      await handle.chmod(0o10777);

      if (!common.isWindows) {
        await chown(dest, process.getuid(), process.getgid());
        await handle.chown(process.getuid(), process.getgid());
      }

      assert.rejects(
        async () => {
          await chown(dest, 1, -1);
        },
        {
          code: 'ERR_OUT_OF_RANGE',
          name: 'RangeError',
          message: 'The value of "gid" is out of range. ' +
                  'It must be >= 0 && < 4294967296. Received -1'
        });

      assert.rejects(
        async () => {
          await handle.chown(1, -1);
        },
        {
          code: 'ERR_OUT_OF_RANGE',
          name: 'RangeError',
          message: 'The value of "gid" is out of range. ' +
                    'It must be >= 0 && < 4294967296. Received -1'
        });

      await handle.close();
    }

    // Set modification times
    {
      const handle = await getHandle(dest);

      await utimes(dest, new Date(), new Date());

      try {
        await handle.utimes(new Date(), new Date());
      } catch (err) {
        // Some systems do not have futimes. If there is an error,
        // expect it to be ENOSYS
        common.expectsError({
          code: 'ENOSYS',
          name: 'Error'
        })(err);
      }

      await handle.close();
    }

    // create symlink
    {
      const newPath = path.resolve(tmpDir, 'baz2.js');
      await rename(dest, newPath);
      let stats = await stat(newPath);
      verifyStatObject(stats);

      if (common.canCreateSymLink()) {
        const newLink = path.resolve(tmpDir, 'baz3.js');
        await symlink(newPath, newLink);
        if (!common.isWindows) {
          await lchown(newLink, process.getuid(), process.getgid());
        }
        stats = await lstat(newLink);
        verifyStatObject(stats);

        assert.strictEqual(newPath.toLowerCase(),
                           (await realpath(newLink)).toLowerCase());
        assert.strictEqual(newPath.toLowerCase(),
                           (await readlink(newLink)).toLowerCase());

        const newMode = 0o666;
        if (common.isOSX) {
          // `lchmod` is only available on macOS.
          await lchmod(newLink, newMode);
          stats = await lstat(newLink);
          assert.strictEqual(stats.mode & 0o777, newMode);
        } else {
          await Promise.all([
            assert.rejects(
              lchmod(newLink, newMode),
              common.expectsError({
                code: 'ERR_METHOD_NOT_IMPLEMENTED',
                name: 'Error',
                message: 'The lchmod() method is not implemented'
              })
            )
          ]);
        }

        await unlink(newLink);
      }
    }

    // create hard link
    {
      const newPath = path.resolve(tmpDir, 'baz2.js');
      const newLink = path.resolve(tmpDir, 'baz4.js');
      await link(newPath, newLink);

      await unlink(newLink);
    }

    // Testing readdir lists both files and directories
    {
      const newDir = path.resolve(tmpDir, 'dir');
      const newFile = path.resolve(tmpDir, 'foo.js');

      await mkdir(newDir);
      await writeFile(newFile, 'DAWGS WIN!', 'utf8');

      const stats = await stat(newDir);
      assert(stats.isDirectory());
      const list = await readdir(tmpDir);
      assert.notStrictEqual(list.indexOf('dir'), -1);
      assert.notStrictEqual(list.indexOf('foo.js'), -1);
      await rmdir(newDir);
      await unlink(newFile);
    }

    // `mkdir` when options is number.
    {
      const dir = path.join(tmpDir, nextdir());
      await mkdir(dir, 777);
      const stats = await stat(dir);
      assert(stats.isDirectory());
    }

    // `mkdir` when options is string.
    {
      const dir = path.join(tmpDir, nextdir());
      await mkdir(dir, '777');
      const stats = await stat(dir);
      assert(stats.isDirectory());
    }

    // `mkdirp` when folder does not yet exist.
    {
      const dir = path.join(tmpDir, nextdir(), nextdir());
      await mkdir(dir, { recursive: true });
      const stats = await stat(dir);
      assert(stats.isDirectory());
    }

    // `mkdirp` when path is a file.
    {
      const dir = path.join(tmpDir, nextdir(), nextdir());
      await mkdir(path.dirname(dir));
      await writeFile(dir, '');
      assert.rejects(
        mkdir(dir, { recursive: true }),
        {
          code: 'EEXIST',
          message: /EEXIST: .*mkdir/,
          name: 'Error',
          syscall: 'mkdir',
        }
      );
    }

    // `mkdirp` when part of the path is a file.
    {
      const file = path.join(tmpDir, nextdir(), nextdir());
      const dir = path.join(file, nextdir(), nextdir());
      await mkdir(path.dirname(file));
      await writeFile(file, '');
      assert.rejects(
        mkdir(dir, { recursive: true }),
        {
          code: 'ENOTDIR',
          message: /ENOTDIR: .*mkdir/,
          name: 'Error',
          syscall: 'mkdir',
        }
      );
    }

    // mkdirp ./
    {
      const dir = path.resolve(tmpDir, `${nextdir()}/./${nextdir()}`);
      await mkdir(dir, { recursive: true });
      const stats = await stat(dir);
      assert(stats.isDirectory());
    }

    // mkdirp ../
    {
      const dir = path.resolve(tmpDir, `${nextdir()}/../${nextdir()}`);
      await mkdir(dir, { recursive: true });
      const stats = await stat(dir);
      assert(stats.isDirectory());
    }

    // fs.mkdirp requires the recursive option to be of type boolean.
    // Everything else generates an error.
    {
      const dir = path.join(tmpDir, nextdir(), nextdir());
      ['', 1, {}, [], null, Symbol('test'), () => {}].forEach((recursive) => {
        assert.rejects(
          // mkdir() expects to get a boolean value for options.recursive.
          async () => mkdir(dir, { recursive }),
          {
            code: 'ERR_INVALID_ARG_TYPE',
            name: 'TypeError'
          }
        );
      });
    }

    // `mkdtemp` with invalid numeric prefix
    {
      await mkdtemp(path.resolve(tmpDir, 'FOO'));
      assert.rejects(
        // mkdtemp() expects to get a string prefix.
        async () => mkdtemp(1),
        {
          code: 'ERR_INVALID_ARG_TYPE',
          name: 'TypeError'
        }
      );
    }

  }

  doTest().then(common.mustCall());
}
