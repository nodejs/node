'use strict';

// Checks that every public fs operation publishes to its tracing
// channel, in all of its forms. A new public function must be added to
// the tables below, or to the list of functions that do not publish.

const common = require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const fs = require('node:fs');
const fsp = require('node:fs/promises');
const path = require('node:path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const file = tmpdir.resolve('file.txt');
const now = new Date();
const { uid, gid } = (() => {
  fs.writeFileSync(file, 'hello world');
  return fs.statSync(file);
})();

let counter = 0;
function fresh(name) {
  return tmpdir.resolve(`${name}-${counter++}`);
}

function newFile() {
  const target = fresh('file');
  fs.writeFileSync(target, 'hello world');
  return target;
}

function newEmptyDir() {
  const target = fresh('empty');
  fs.mkdirSync(target);
  return target;
}

function newDir() {
  const target = fresh('dir');
  fs.mkdirSync(target);
  fs.writeFileSync(path.join(target, 'a.txt'), 'a');
  return target;
}

function newLink() {
  const target = fresh('link');
  try {
    fs.symlinkSync(file, target);
  } catch {
    // Symbolic links can need extra privileges on Windows. The
    // operation then fails, which still publishes events.
  }
  return target;
}

// Not file system operations, or covered by the operations they use.
const notTraced = new Set([
  'Dir', 'Dirent', 'FileReadStream', 'FileWriteStream', 'ReadStream',
  'Stats', 'Utf8Stream', 'WriteStream', '_toUnixTimestamp',
  'createReadStream', 'createWriteStream', 'unwatchFile', 'watch',
  'watchFile',
]);
const notTracedPromises = new Set(['constants', 'glob', 'watch']);

// [export name, channel name, call]
const syncOps = [
  ['accessSync', 'access', () => fs.accessSync(file)],
  ['appendFileSync', 'appendFile', () => fs.appendFileSync(newFile(), 'x')],
  ['chmodSync', 'chmod', () => fs.chmodSync(newFile(), 0o644)],
  ['chownSync', 'chown', () => fs.chownSync(newFile(), uid, gid)],
  ['closeSync', 'close', () => fs.closeSync(fs.openSync(file))],
  ['copyFileSync', 'copyFile', () => fs.copyFileSync(file, fresh('copy'))],
  ['cpSync', 'cp', () => fs.cpSync(file, fresh('cp'))],
  ['existsSync', 'exists', () => fs.existsSync(file)],
  ['fchmodSync', 'fchmod', (fd) => fs.fchmodSync(fd, 0o644)],
  ['fchownSync', 'fchown', (fd) => fs.fchownSync(fd, uid, gid)],
  ['fdatasyncSync', 'fdatasync', (fd) => fs.fdatasyncSync(fd)],
  ['fstatSync', 'fstat', (fd) => fs.fstatSync(fd)],
  ['fsyncSync', 'fsync', (fd) => fs.fsyncSync(fd)],
  ['ftruncateSync', 'ftruncate', (fd) => fs.ftruncateSync(fd, 11)],
  ['futimesSync', 'futimes', (fd) => fs.futimesSync(fd, now, now)],
  ['globSync', 'glob', () => fs.globSync('*', { cwd: newDir() })],
  ['lchmodSync', 'lchmod', () => fs.lchmodSync(newLink(), 0o644)],
  ['lchownSync', 'lchown', () => fs.lchownSync(newLink(), uid, gid)],
  ['linkSync', 'link', () => fs.linkSync(file, fresh('hardlink'))],
  ['lstatSync', 'lstat', () => fs.lstatSync(file)],
  ['lutimesSync', 'lutimes', () => fs.lutimesSync(newLink(), now, now)],
  ['mkdirSync', 'mkdir', () => fs.mkdirSync(fresh('mkdir'))],
  ['mkdtempSync', 'mkdtemp', () => fs.mkdtempSync(fresh('tmp'))],
  ['mkdtempDisposableSync', 'mkdtemp',
   () => fs.mkdtempDisposableSync(fresh('tmp')).remove()],
  ['openAsBlobSync', 'openAsBlob', () => fs.openAsBlobSync(file)],
  ['openSync', 'open', () => fs.closeSync(fs.openSync(file))],
  ['opendirSync', 'opendir', () => fs.opendirSync(newDir()).closeSync()],
  ['readFileSync', 'readFile', () => fs.readFileSync(file)],
  ['readSync', 'read', (fd) => fs.readSync(fd, Buffer.alloc(4), 0, 4, 0)],
  ['readdirSync', 'readdir', () => fs.readdirSync(newDir())],
  ['readlinkSync', 'readlink', () => fs.readlinkSync(newLink())],
  ['readvSync', 'readv', (fd) => fs.readvSync(fd, [Buffer.alloc(4)], 0)],
  ['realpathSync', 'realpath', () => fs.realpathSync(file)],
  ['realpathSync.native', 'realpath', () => fs.realpathSync.native(file)],
  ['renameSync', 'rename', () => fs.renameSync(newFile(), fresh('renamed'))],
  ['rmSync', 'rm', () => fs.rmSync(newDir(), { recursive: true })],
  ['rmdirSync', 'rmdir', () => fs.rmdirSync(newEmptyDir())],
  ['statSync', 'stat', () => fs.statSync(file)],
  ['statfsSync', 'statfs', () => fs.statfsSync(file)],
  ['symlinkSync', 'symlink', () => fs.symlinkSync(file, fresh('symlink'))],
  ['truncateSync', 'truncate', () => fs.truncateSync(newFile(), 2)],
  ['unlinkSync', 'unlink', () => fs.unlinkSync(newFile())],
  ['utimesSync', 'utimes', () => fs.utimesSync(file, now, now)],
  ['writeFileSync', 'writeFile', () => fs.writeFileSync(fresh('written'), 'x')],
  ['writeSync', 'write', (fd) => fs.writeSync(fd, Buffer.from('x'))],
  ['writevSync', 'writev', (fd) => fs.writevSync(fd, [Buffer.from('x')])],
  ['Dir.closeSync', 'closedir', (fd, dir) => dir.closeSync()],
];

const callbackOps = [
  ['access', 'access', (cb) => fs.access(file, cb)],
  ['appendFile', 'appendFile', (cb) => fs.appendFile(newFile(), 'x', cb)],
  ['chmod', 'chmod', (cb) => fs.chmod(newFile(), 0o644, cb)],
  ['chown', 'chown', (cb) => fs.chown(newFile(), uid, gid, cb)],
  ['close', 'close', (cb) => fs.close(fs.openSync(file), cb)],
  ['copyFile', 'copyFile', (cb) => fs.copyFile(file, fresh('copy'), cb)],
  ['cp', 'cp', (cb) => fs.cp(file, fresh('cp'), cb)],
  ['exists', 'exists', (cb) => fs.exists(file, () => cb())],
  ['fchmod', 'fchmod', (cb, fd) => fs.fchmod(fd, 0o644, cb)],
  ['fchown', 'fchown', (cb, fd) => fs.fchown(fd, uid, gid, cb)],
  ['fdatasync', 'fdatasync', (cb, fd) => fs.fdatasync(fd, cb)],
  ['fstat', 'fstat', (cb, fd) => fs.fstat(fd, cb)],
  ['fsync', 'fsync', (cb, fd) => fs.fsync(fd, cb)],
  ['ftruncate', 'ftruncate', (cb, fd) => fs.ftruncate(fd, 11, cb)],
  ['futimes', 'futimes', (cb, fd) => fs.futimes(fd, now, now, cb)],
  ['glob', 'glob', (cb) => fs.glob('*', { cwd: newDir() }, cb)],
  ['lchmod', 'lchmod', (cb) => fs.lchmod(newLink(), 0o644, cb)],
  ['lchown', 'lchown', (cb) => fs.lchown(newLink(), uid, gid, cb)],
  ['link', 'link', (cb) => fs.link(file, fresh('hardlink'), cb)],
  ['lstat', 'lstat', (cb) => fs.lstat(file, cb)],
  ['lutimes', 'lutimes', (cb) => fs.lutimes(newLink(), now, now, cb)],
  ['mkdir', 'mkdir', (cb) => fs.mkdir(fresh('mkdir'), cb)],
  ['mkdtemp', 'mkdtemp', (cb) => fs.mkdtemp(fresh('tmp'), cb)],
  ['open', 'open', (cb) => fs.open(file, (err, fd) => {
    if (fd !== undefined) fs.closeSync(fd);
    cb(err);
  })],
  ['opendir', 'opendir', (cb) => fs.opendir(newDir(), (err, dir) => {
    dir?.closeSync();
    cb(err);
  })],
  ['read', 'read', (cb, fd) => fs.read(fd, Buffer.alloc(4), 0, 4, 0, cb)],
  ['readFile', 'readFile', (cb) => fs.readFile(file, cb)],
  ['readdir', 'readdir', (cb) => fs.readdir(newDir(), cb)],
  ['readlink', 'readlink', (cb) => fs.readlink(newLink(), cb)],
  ['readv', 'readv', (cb, fd) => fs.readv(fd, [Buffer.alloc(4)], 0, cb)],
  ['realpath', 'realpath', (cb) => fs.realpath(file, cb)],
  ['realpath.native', 'realpath', (cb) => fs.realpath.native(file, cb)],
  ['rename', 'rename', (cb) => fs.rename(newFile(), fresh('renamed'), cb)],
  ['rm', 'rm', (cb) => fs.rm(newDir(), { recursive: true }, cb)],
  ['rmdir', 'rmdir', (cb) => fs.rmdir(newEmptyDir(), cb)],
  ['stat', 'stat', (cb) => fs.stat(file, cb)],
  ['statfs', 'statfs', (cb) => fs.statfs(file, cb)],
  ['symlink', 'symlink', (cb) => fs.symlink(file, fresh('symlink'), cb)],
  ['truncate', 'truncate', (cb) => fs.truncate(newFile(), 2, cb)],
  ['unlink', 'unlink', (cb) => fs.unlink(newFile(), cb)],
  ['utimes', 'utimes', (cb) => fs.utimes(file, now, now, cb)],
  ['write', 'write', (cb, fd) => fs.write(fd, Buffer.from('x'), cb)],
  ['writeFile', 'writeFile', (cb) => fs.writeFile(fresh('written'), 'x', cb)],
  ['writev', 'writev', (cb, fd) => fs.writev(fd, [Buffer.from('x')], cb)],
  ['Dir.close', 'closedir', (cb, fd, dir) => dir.close(cb)],
];

// FileHandle methods are listed with a `FileHandle.` prefix.
const promiseOps = [
  ['access', 'access', () => fsp.access(file)],
  ['appendFile', 'appendFile', () => fsp.appendFile(newFile(), 'x')],
  ['chmod', 'chmod', () => fsp.chmod(newFile(), 0o644)],
  ['chown', 'chown', () => fsp.chown(newFile(), uid, gid)],
  ['copyFile', 'copyFile', () => fsp.copyFile(file, fresh('copy'))],
  ['cp', 'cp', () => fsp.cp(file, fresh('cp'))],
  ['lchmod', 'lchmod', () => fsp.lchmod(newLink(), 0o644)],
  ['lchown', 'lchown', () => fsp.lchown(newLink(), uid, gid)],
  ['link', 'link', () => fsp.link(file, fresh('hardlink'))],
  ['lstat', 'lstat', () => fsp.lstat(file)],
  ['lutimes', 'lutimes', () => fsp.lutimes(newLink(), now, now)],
  ['mkdir', 'mkdir', () => fsp.mkdir(fresh('mkdir'))],
  ['mkdtemp', 'mkdtemp', () => fsp.mkdtemp(fresh('tmp'))],
  ['mkdtempDisposable', 'mkdtemp',
   async () => (await fsp.mkdtempDisposable(fresh('tmp'))).remove()],
  ['open', 'open', async () => (await fsp.open(file)).close()],
  ['opendir', 'opendir', async () => (await fsp.opendir(newDir())).close()],
  ['openAsBlob', 'openAsBlob', () => fs.openAsBlob(file)],
  ['readFile', 'readFile', () => fsp.readFile(file)],
  ['readdir', 'readdir', () => fsp.readdir(newDir())],
  ['readlink', 'readlink', () => fsp.readlink(newLink())],
  ['realpath', 'realpath', () => fsp.realpath(file)],
  ['rename', 'rename', () => fsp.rename(newFile(), fresh('renamed'))],
  ['rm', 'rm', () => fsp.rm(newDir(), { recursive: true })],
  ['rmdir', 'rmdir', () => fsp.rmdir(newEmptyDir())],
  ['stat', 'stat', () => fsp.stat(file)],
  ['statfs', 'statfs', () => fsp.statfs(file)],
  ['symlink', 'symlink', () => fsp.symlink(file, fresh('symlink'))],
  ['truncate', 'truncate', () => fsp.truncate(newFile(), 2)],
  ['unlink', 'unlink', () => fsp.unlink(newFile())],
  ['utimes', 'utimes', () => fsp.utimes(file, now, now)],
  ['writeFile', 'writeFile', () => fsp.writeFile(fresh('written'), 'x')],
  ['FileHandle.appendFile', 'writeFile', (fh) => fh.appendFile('x')],
  ['FileHandle.chmod', 'fchmod', (fh) => fh.chmod(0o644)],
  ['FileHandle.chown', 'fchown', (fh) => fh.chown(uid, gid)],
  ['FileHandle.close', 'close', (fh) => fh.close()],
  ['FileHandle.datasync', 'fdatasync', (fh) => fh.datasync()],
  ['FileHandle.read', 'read', (fh) => fh.read(Buffer.alloc(4), 0, 4, 0)],
  ['FileHandle.readFile', 'readFile', (fh) => fh.readFile()],
  ['FileHandle.readv', 'readv', (fh) => fh.readv([Buffer.alloc(4)], 0)],
  ['FileHandle.stat', 'fstat', (fh) => fh.stat()],
  ['FileHandle.sync', 'fsync', (fh) => fh.sync()],
  ['FileHandle.truncate', 'ftruncate', (fh) => fh.truncate(11)],
  ['FileHandle.utimes', 'futimes', (fh) => fh.utimes(now, now)],
  ['FileHandle.write', 'write', (fh) => fh.write(Buffer.from('x'))],
  ['FileHandle.writeFile', 'writeFile', (fh) => fh.writeFile('x')],
  ['FileHandle.writev', 'writev', (fh) => fh.writev([Buffer.from('x')])],
  ['Dir.close', 'closedir', () => fs.opendirSync(newDir()).close()],
];

function supported(name) {
  // The lchmod functions only exist where O_SYMLINK does.
  return !name.includes('lchmod') || fs.lchmod !== undefined;
}

function listed(ops) {
  return new Set(ops.map(([name]) => name));
}

{
  // Every public function is listed.
  const sync = listed(syncOps);
  const callback = listed(callbackOps);
  const promise = listed(promiseOps);
  for (const [name, value] of Object.entries(fs)) {
    if (typeof value !== 'function' || notTraced.has(name)) continue;
    if (!supported(name)) continue;
    // The fs.openAsBlob() function returns a promise.
    assert.ok(sync.has(name) || callback.has(name) || promise.has(name),
              `fs.${name} is not listed`);
  }
  for (const [name, value] of Object.entries(fsp)) {
    if (typeof value !== 'function' || notTracedPromises.has(name)) continue;
    if (!supported(name)) continue;
    assert.ok(promise.has(name), `fs.promises.${name} is not listed`);
  }
}

function watch(channelName) {
  const events = [];
  const channel = dc.tracingChannel(`fs.${channelName}`);
  const handlers = {};
  for (const type of ['start', 'end', 'asyncStart', 'asyncEnd', 'error']) {
    handlers[type] = (context) => events.push({ type, context });
  }
  channel.subscribe(handlers);
  return { events, stop: () => channel.unsubscribe(handlers) };
}

// Symbolic links can need extra privileges on Windows, so operations on
// them may fail there.
const mayFail = new Set(common.isWindows ?
  ['lchmod', 'lchown', 'lutimes', 'readlink', 'symlink'] : []);

function check(label, channelName, events, api) {
  const starts = events.filter((e) => e.type === 'start' && e.context.api === api);
  assert.strictEqual(starts.length, 1,
                     `${label} published ${starts.length} times with api '${api}'`);
  const { context } = starts[0];
  const types = events.filter((e) => e.context === context).map((e) => e.type);
  const expected = api === 'sync' ?
    ['start', 'end'] :
    ['start', 'end', 'asyncStart', 'asyncEnd'];
  assert.deepStrictEqual(types.filter((t) => t !== 'error'), expected, label);
  if (!mayFail.has(channelName)) {
    assert.strictEqual(context.error, undefined,
                       `${label} failed: ${context.error?.message}`);
  }
}

async function run() {
  for (const [name, channelName, call] of syncOps) {
    if (!supported(name)) continue;
    const fd = fs.openSync(newFile(), 'r+');
    const dir = fs.opendirSync(newDir());
    const { events, stop } = watch(channelName);
    try {
      call(fd, dir);
    } catch {
      // check() looks at the error.
    }
    stop();
    fs.closeSync(fd);
    try { dir.closeSync(); } catch { /* Closed by the operation. */ }
    check(`fs.${name}`, channelName, events, 'sync');
  }

  for (const [name, channelName, call] of callbackOps) {
    if (!supported(name)) continue;
    const fd = fs.openSync(newFile(), 'r+');
    const dir = fs.opendirSync(newDir());
    const { events, stop } = watch(channelName);
    await new Promise((resolve) => call(() => setImmediate(resolve), fd, dir));
    stop();
    fs.closeSync(fd);
    try { dir.closeSync(); } catch { /* Closed by the operation. */ }
    check(`fs.${name}`, channelName, events, 'callback');
  }

  for (const [name, channelName, call] of promiseOps) {
    if (!supported(name)) continue;
    const fh = await fsp.open(newFile(), 'r+');
    const { events, stop } = watch(channelName);
    try {
      await call(fh);
    } catch {
      // check() looks at the error.
    }
    stop();
    await fh.close();
    check(`fs.promises.${name}`, channelName, events, 'promise');
  }
}

run().then(common.mustCall());
