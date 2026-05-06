'use strict';

require('../common');
const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
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
