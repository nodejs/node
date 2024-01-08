'use strict';

const common = require('../common');
const { mkdtempSync, watch, writeFileSync } = require('node:fs');
const { tmpdir } = require('node:os');
const { join } = require('node:path');
const tmpDir = mkdtempSync(join(tmpdir(), 'repro-test-'));
const filename = join(tmpDir, 'test.file');
const assert = require('assert');

const keepalive = setTimeout(() => {
  throw new Error('timed out');
}, common.platformTimeout(30_0000));

const watcher = watch(tmpDir, { recursive: true }, common.mustCall((eventType, _filename) => {
  clearTimeout(keepalive);
  watcher.close();
  assert.strictEqual(eventType, 'rename');
  assert.strictEqual(join(tmpDir, _filename), filename);
  console.log(eventType, filename);
}));

writeFileSync(filename, 'foobar2');
