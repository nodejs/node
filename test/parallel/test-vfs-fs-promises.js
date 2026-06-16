// Flags: --experimental-vfs
'use strict';

// fs/promises dispatches through VFS for each supported path-based and
// FileHandle-based operation.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const fsp = require('fs/promises');
const path = require('path');
const vfs = require('node:vfs');

(async () => {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.writeFileSync('/src/hello.txt', 'hello world');
  const mountPoint = myVfs.mount();
  const p = (s) => path.join(mountPoint, s);

  // Path-based reads
  assert.strictEqual((await fsp.stat(p('src/hello.txt'))).isFile(), true);
  assert.strictEqual((await fsp.lstat(p('src/hello.txt'))).isFile(), true);
  assert.ok((await fsp.readdir(p('src'))).includes('hello.txt'));
  assert.strictEqual(await fsp.readFile(p('src/hello.txt'), 'utf8'),
                     'hello world');
  assert.strictEqual(await fsp.realpath(p('src/hello.txt')),
                     p('src/hello.txt'));
  await fsp.access(p('src/hello.txt'));

  // statfs
  const sfs = await fsp.statfs(p('src/hello.txt'));
  assert.strictEqual(typeof sfs.bsize, 'number');

  // Path-based writes
  await fsp.writeFile(p('src/pw.txt'), 'pdata');
  assert.strictEqual(fs.readFileSync(p('src/pw.txt'), 'utf8'), 'pdata');
  await fsp.appendFile(p('src/pw.txt'), ' more');
  assert.strictEqual(fs.readFileSync(p('src/pw.txt'), 'utf8'), 'pdata more');

  await fsp.mkdir(p('src/pd'));
  await fsp.rmdir(p('src/pd'));
  await fsp.rm(p('src/pw.txt'));
  assert.strictEqual(fs.existsSync(p('src/pw.txt')), false);

  await fsp.copyFile(p('src/hello.txt'), p('src/pcopy.txt'));
  assert.strictEqual(fs.readFileSync(p('src/pcopy.txt'), 'utf8'),
                     'hello world');

  await fsp.rename(p('src/pcopy.txt'), p('src/prenamed.txt'));
  assert.strictEqual(fs.existsSync(p('src/pcopy.txt')), false);
  await fsp.unlink(p('src/prenamed.txt'));

  await fsp.symlink('hello.txt', p('src/plnk.txt'));
  assert.strictEqual(await fsp.readlink(p('src/plnk.txt')), 'hello.txt');

  await fsp.truncate(p('src/hello.txt'), 5);
  assert.strictEqual(fs.readFileSync(p('src/hello.txt'), 'utf8'), 'hello');

  await fsp.link(p('src/hello.txt'), p('src/plink.txt'));
  assert.strictEqual(fs.readFileSync(p('src/plink.txt'), 'utf8'), 'hello');

  const tmp = await fsp.mkdtemp(p('src/ptmp-'));
  assert.ok(tmp.startsWith(p('src/ptmp-')));
  assert.strictEqual(fs.statSync(tmp).isDirectory(), true);

  // Attribute mutations
  const uid = process.getuid?.() ?? 0;
  const gid = process.getgid?.() ?? 0;
  const now = new Date();
  await fsp.chmod(p('src/hello.txt'), 0o644);
  await fsp.chown(p('src/hello.txt'), uid, gid);
  await fsp.lchown(p('src/hello.txt'), uid, gid);
  await fsp.utimes(p('src/hello.txt'), now, now);
  await fsp.lutimes(p('src/hello.txt'), now, now);

  await fsp.lchmod(p('src/plnk.txt'), 0o700);
  assert.strictEqual(
    (await fsp.lstat(p('src/hello.txt'))).mode & 0o777,
    0o644,
  );
  assert.strictEqual(
    (await fsp.lstat(p('src/plnk.txt'))).mode & 0o777,
    0o700,
  );

  // FileHandle via fsp.open
  const handle = await fsp.open(p('src/hello.txt'), 'r');
  assert.strictEqual(await handle.readFile('utf8'), 'hello');
  await handle.close();

  myVfs.unmount();
})().then(common.mustCall());
