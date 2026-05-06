'use strict';

require('../common');
const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const fsPromises = require('node:fs/promises');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const filePath = tmpdir.resolve('temp.txt');
fs.writeFileSync(filePath, 'Test');

test('fs.stat aborts when signal is already aborted', async () => {
  const signal = AbortSignal.abort();
  const { promise, resolve, reject } = Promise.withResolvers();
  fs.stat(filePath, { signal }, (err, stats) => {
    if (err) return reject(err);
    resolve(stats);
  });
  await assert.rejects(promise, { name: 'AbortError' });
});

test('fs.stat aborts in-flight when signal aborts after the call', async () => {
  const controller = new AbortController();
  const { promise, resolve, reject } = Promise.withResolvers();
  fs.stat(filePath, { signal: controller.signal }, (err, stats) => {
    if (err) return reject(err);
    resolve(stats);
  });
  controller.abort();
  await assert.rejects(promise, { name: 'AbortError' });
});

test('fs.stat throws ERR_INVALID_ARG_TYPE for invalid signal', () => {
  assert.throws(
    () => fs.stat(filePath, { signal: 'not-a-signal' }, () => {}),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('fs.lstat aborts in-flight when signal aborts after the call', async () => {
  const controller = new AbortController();
  const { promise, resolve, reject } = Promise.withResolvers();
  fs.lstat(filePath, { signal: controller.signal }, (err, stats) => {
    if (err) return reject(err);
    resolve(stats);
  });
  controller.abort();
  await assert.rejects(promise, { name: 'AbortError' });
});

test('fs.fstat aborts in-flight when signal aborts after the call', async () => {
  const fd = fs.openSync(filePath, 'r');
  try {
    const controller = new AbortController();
    const { promise, resolve, reject } = Promise.withResolvers();
    fs.fstat(fd, { signal: controller.signal }, (err, stats) => {
      if (err) return reject(err);
      resolve(stats);
    });
    controller.abort();
    await assert.rejects(promise, { name: 'AbortError' });
  } finally {
    fs.closeSync(fd);
  }
});

test('fsPromises.stat rejects when signal is already aborted', async () => {
  const signal = AbortSignal.abort();
  await assert.rejects(fsPromises.stat(filePath, { signal }), { name: 'AbortError' });
});

test('fsPromises.stat rejects in-flight when signal aborts after the call', async () => {
  const controller = new AbortController();
  const p = fsPromises.stat(filePath, { signal: controller.signal });
  controller.abort();
  await assert.rejects(p, { name: 'AbortError' });
});

test('fsPromises.stat rejects with ERR_INVALID_ARG_TYPE for invalid signal', async () => {
  await assert.rejects(
    fsPromises.stat(filePath, { signal: 'not-a-signal' }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('fsPromises.lstat rejects in-flight when signal aborts after the call', async () => {
  const controller = new AbortController();
  const p = fsPromises.lstat(filePath, { signal: controller.signal });
  controller.abort();
  await assert.rejects(p, { name: 'AbortError' });
});

test('filehandle.stat rejects in-flight when signal aborts after the call', async () => {
  const fh = await fsPromises.open(filePath, 'r');
  try {
    const controller = new AbortController();
    const p = fh.stat({ signal: controller.signal });
    controller.abort();
    await assert.rejects(p, { name: 'AbortError' });
  } finally {
    await fh.close();
  }
});
