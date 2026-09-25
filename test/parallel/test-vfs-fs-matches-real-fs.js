// Flags: --experimental-vfs
'use strict';

// `node:fs` entry points route mounted paths through the VFS hooks. Where a
// hook is missing, runs before argument validation, or ignores the
// descriptor form of an operation, the same call behaves differently from a
// real path. Each case states the real-fs outcome as the expectation. Cases
// are independent so the runner reports each one.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');
const { test } = require('node:test');

function mount(populate) {
  const layer = vfs.create();
  populate?.(layer);
  return layer.mount();
}

test('watchFile on a mounted path installs a stat watcher', () => {
  const file = path.join(mount((l) => l.writeFileSync('/f', 'x')), 'f');
  fs.watchFile(file, { interval: 10 }, common.mustNotCall());
  fs.unwatchFile(file);
});

test('fs.promises.watch on a mounted directory yields events', async () => {
  const dir = path.join(mount((l) => l.mkdirSync('/d')), 'd');
  const ac = new AbortController();
  const watcher = fs.promises.watch(dir, { signal: ac.signal });
  setTimeout(() => fs.writeFileSync(path.join(dir, 'x'), '1'), 20);
  for await (const event of watcher) {
    assert.strictEqual(event.filename, 'x');
    ac.abort();
    break;
  }
});

test('watch on a missing mounted path throws ENOENT', () => {
  const dir = mount();
  // Should the call return a watcher instead, it polls forever, so it is
  // closed to let the process exit.
  let watcher;
  try {
    assert.throws(() => { watcher = fs.watch(path.join(dir, 'nope')); },
                  { code: 'ENOENT' });
  } finally {
    watcher?.close();
  }
});

test('utimesSync accepts numeric strings as seconds', () => {
  const file = path.join(mount((l) => l.writeFileSync('/f', 'x')), 'f');
  fs.utimesSync(file, '1000', '2000');
  assert.strictEqual(fs.statSync(file).mtimeMs, 2000 * 1000);
});

test('utimesSync rejects an invalid time argument', () => {
  const file = path.join(mount((l) => l.writeFileSync('/f', 'x')), 'f');
  assert.throws(() => fs.utimesSync(file, {}, {}), { code: 'ERR_INVALID_ARG_TYPE' });
});

test('readdirSync rejects an invalid encoding', () => {
  const dir = mount();
  assert.throws(() => fs.readdirSync(dir, { encoding: 'nope' }),
                { code: 'ERR_INVALID_ARG_VALUE' });
});

test('futimesSync updates the timestamps through a descriptor', () => {
  const file = path.join(mount((l) => l.writeFileSync('/f', 'x')), 'f');
  const fd = fs.openSync(file, 'r+');
  try {
    fs.futimesSync(fd, 1000, 2000);
  } finally {
    fs.closeSync(fd);
  }
  assert.strictEqual(fs.statSync(file).mtimeMs, 2000 * 1000);
});

test('fchmodSync changes the mode through a descriptor', () => {
  const file = path.join(mount((l) => l.writeFileSync('/f', 'x')), 'f');
  const fd = fs.openSync(file, 'r+');
  try {
    fs.fchmodSync(fd, 0o600);
  } finally {
    fs.closeSync(fd);
  }
  assert.strictEqual(fs.statSync(file).mode & 0o777, 0o600);
});

test('mkdtempSync with a trailing separator creates the directory inside the prefix', () => {
  const dir = path.join(mount((l) => l.mkdirSync('/dir')), 'dir');
  const created = fs.mkdtempSync(dir + path.sep);
  assert.ok(created.startsWith(dir + path.sep), `${created} is not inside ${dir}`);
  assert.strictEqual(fs.statSync(created).isDirectory(), true);
});

test('mkdirSync({ recursive: true }) returns the first directory created', () => {
  const dir = mount();
  const created = fs.mkdirSync(path.join(dir, 'a', 'b'), { recursive: true });
  assert.strictEqual(created, path.join(dir, 'a'));
});

test('a closed Dir can be disposed asynchronously', async () => {
  const dir = mount((l) => l.mkdirSync('/d'));
  const handle = fs.opendirSync(dir);
  handle.closeSync();
  await handle[Symbol.asyncDispose]();
});
