'use strict';

const common = require('../common');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

if (common.isAIX)
  common.skip('folder watch capability is limited in AIX.');

if (common.isSunOS)
  common.skip('fs.watch is not reliable on SunOS.');

const assert = require('assert');
const path = require('path');
const fs = require('fs/promises');
const fsSync = require('fs');

const tmpdir = require('../common/tmpdir');
const testDir = tmpdir.path;
tmpdir.refresh();

// Test 1: String glob pattern ignore with Promise API
(async function testGlobIgnore() {
  const testsubdir = await fs.mkdtemp(testDir + path.sep);
  const keepFile = 'keep.txt';
  const ignoreFile = 'ignore.log';
  const keepFilePath = path.join(testsubdir, keepFile);
  const ignoreFilePath = path.join(testsubdir, ignoreFile);

  const watcher = fs.watch(testsubdir, { ignore: '*.log' });

  let seenIgnored = false;
  let interval;

  process.on('exit', () => {
    assert.ok(interval === null);
    assert.strictEqual(seenIgnored, false);
  });

  process.nextTick(common.mustCall(() => {
    interval = setInterval(() => {
      fsSync.writeFileSync(ignoreFilePath, 'ignored');
      fsSync.writeFileSync(keepFilePath, 'content-' + Date.now());
    }, 100);
  }));

  for await (const { filename } of watcher) {
    if (filename === ignoreFile) {
      seenIgnored = true;
    }
    if (filename === keepFile) {
      break;
    }
  }

  clearInterval(interval);
  interval = null;
})().then(common.mustCall());

// Test 2: RegExp ignore pattern with Promise API
(async function testRegExpIgnore() {
  const testsubdir = await fs.mkdtemp(testDir + path.sep);
  const keepFile = 'keep.txt';
  const ignoreFile = 'ignore.tmp';
  const keepFilePath = path.join(testsubdir, keepFile);
  const ignoreFilePath = path.join(testsubdir, ignoreFile);

  const watcher = fs.watch(testsubdir, { ignore: /\.tmp$/ });

  let seenIgnored = false;
  let interval;

  process.on('exit', () => {
    assert.ok(interval === null);
    assert.strictEqual(seenIgnored, false);
  });

  process.nextTick(common.mustCall(() => {
    interval = setInterval(() => {
      fsSync.writeFileSync(ignoreFilePath, 'ignored');
      fsSync.writeFileSync(keepFilePath, 'content-' + Date.now());
    }, 100);
  }));

  for await (const { filename } of watcher) {
    if (filename === ignoreFile) {
      seenIgnored = true;
    }
    if (filename === keepFile) {
      break;
    }
  }

  clearInterval(interval);
  interval = null;
})().then(common.mustCall());

// Test 3: Function ignore pattern with Promise API
(async function testFunctionIgnore() {
  const testsubdir = await fs.mkdtemp(testDir + path.sep);
  const keepFile = 'visible.txt';
  const ignoreFile = '.hidden';
  const keepFilePath = path.join(testsubdir, keepFile);
  const ignoreFilePath = path.join(testsubdir, ignoreFile);

  const watcher = fs.watch(testsubdir, {
    ignore: (filename) => filename.startsWith('.'),
  });

  let seenIgnored = false;
  let interval;

  process.on('exit', () => {
    assert.ok(interval === null);
    assert.strictEqual(seenIgnored, false);
  });

  process.nextTick(common.mustCall(() => {
    interval = setInterval(() => {
      fsSync.writeFileSync(ignoreFilePath, 'ignored');
      fsSync.writeFileSync(keepFilePath, 'content-' + Date.now());
    }, 100);
  }));

  for await (const { filename } of watcher) {
    if (filename === ignoreFile) {
      seenIgnored = true;
    }
    if (filename === keepFile) {
      break;
    }
  }

  clearInterval(interval);
  interval = null;
})().then(common.mustCall());

// Test 4: Array of mixed patterns with Promise API
(async function testArrayIgnore() {
  const testsubdir = await fs.mkdtemp(testDir + path.sep);
  const keepFile = 'keep.txt';
  const ignoreLog = 'debug.log';
  const ignoreTmp = 'temp.tmp';
  const keepFilePath = path.join(testsubdir, keepFile);
  const ignoreLogPath = path.join(testsubdir, ignoreLog);
  const ignoreTmpPath = path.join(testsubdir, ignoreTmp);

  const watcher = fs.watch(testsubdir, {
    ignore: [
      '*.log',
      /\.tmp$/,
    ],
  });

  let seenLog = false;
  let seenTmp = false;
  let interval;

  process.on('exit', () => {
    assert.ok(interval === null);
    assert.strictEqual(seenLog, false);
    assert.strictEqual(seenTmp, false);
  });

  process.nextTick(common.mustCall(() => {
    interval = setInterval(() => {
      fsSync.writeFileSync(ignoreLogPath, 'ignored');
      fsSync.writeFileSync(ignoreTmpPath, 'ignored');
      fsSync.writeFileSync(keepFilePath, 'content-' + Date.now());
    }, 100);
  }));

  for await (const { filename } of watcher) {
    if (filename === ignoreLog) seenLog = true;
    if (filename === ignoreTmp) seenTmp = true;
    if (filename === keepFile) {
      break;
    }
  }

  clearInterval(interval);
  interval = null;
})().then(common.mustCall());

// Test 5: Invalid option validation with Promise API
// Note: async generators don't execute until iteration starts,
// so we need to use assert.rejects with iteration
(async function testInvalidIgnore() {
  const testsubdir = await fs.mkdtemp(testDir + path.sep);

  await assert.rejects(
    async () => {
      const watcher = fs.watch(testsubdir, { ignore: 123 });
      // eslint-disable-next-line no-unused-vars
      for await (const _ of watcher) {
        // Will throw before yielding
      }
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );

  await assert.rejects(
    async () => {
      const watcher = fs.watch(testsubdir, { ignore: '' });
      // eslint-disable-next-line no-unused-vars
      for await (const _ of watcher) {
        // Will throw before yielding
      }
    },
    {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
    }
  );
})().then(common.mustCall());
