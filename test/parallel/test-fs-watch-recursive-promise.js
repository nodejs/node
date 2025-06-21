'use strict';

const common = require('../common');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

// fs-watch on folders have limited capability in AIX.
// The testcase makes use of folder watching, and causes
// hang. This behavior is documented. Skip this for AIX.

if (common.isAIX)
  common.skip('folder watch capability is limited in AIX.');

const assert = require('assert');
const path = require('path');
const fs = require('fs/promises');
const fsSync = require('fs');

const tmpdir = require('../common/tmpdir');
const testDir = tmpdir.path;
tmpdir.refresh();

(async function run() {
  // Add a file to already watching folder

  const testsubdir = await fs.mkdtemp(testDir + path.sep);
  const file = '1.txt';
  const filePath = path.join(testsubdir, file);
  const watcher = fs.watch(testsubdir, { recursive: true });

  let interval;

  process.on('exit', function() {
    assert.ok(interval === null, 'watcher Object was not closed');
  });

  process.nextTick(common.mustCall(() => {
    interval = setInterval(() => {
      fsSync.writeFileSync(filePath, 'world');
    }, 500);
  }));

  for await (const payload of watcher) {
    const { eventType, filename } = payload;

    assert.ok(eventType === 'change' || eventType === 'rename');

    if (filename === file) {
      break;
    }
  }

  clearInterval(interval);
  interval = null;
})().then(common.mustCall());

(async function() {
  // Test that aborted AbortSignal are reported.
  const testsubdir = await fs.mkdtemp(testDir + path.sep);
  const error = new Error();
  const watcher = fs.watch(testsubdir, { recursive: true, signal: AbortSignal.abort(error) });
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of watcher);
  }, { code: 'ABORT_ERR', cause: error });
})().then(common.mustCall());

(async function() {
  // Test that with AbortController.
  const testsubdir = await fs.mkdtemp(testDir + path.sep);
  const file = '2.txt';
  const filePath = path.join(testsubdir, file);
  const error = new Error();
  const ac = new AbortController();
  const watcher = fs.watch(testsubdir, { recursive: true, signal: ac.signal });
  let interval;
  process.on('exit', function() {
    assert.ok(interval === null, 'watcher Object was not closed');
  });
  process.nextTick(common.mustCall(() => {
    interval = setInterval(() => {
      fsSync.writeFileSync(filePath, 'world');
    }, 50);
    ac.abort(error);
  }));
  await assert.rejects(async () => {
    for await (const { eventType } of watcher) {
      assert.ok(eventType === 'change' || eventType === 'rename');
    }
  }, { code: 'ABORT_ERR', cause: error });
  clearInterval(interval);
  interval = null;
})().then(common.mustCall());
