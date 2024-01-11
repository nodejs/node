'use strict';

const common = require('../common');
const { mkdtempSync, watch, writeFileSync } = require('node:fs');
const { join } = require('node:path');
const tmpdir = require('../common/tmpdir.js');
const assert = require('assert');

tmpdir.refresh();

const tmpDir = tmpdir.path;
const filename = join(tmpDir, 'test.file');

const keepalive = setTimeout(() => {
  throw new Error('timed out');
}, common.platformTimeout(30_000));

const watcher = watch(tmpDir, { recursive: true }, common.mustCall((eventType, _filename) => {
  clearTimeout(keepalive);
  watcher.close();
  assert.strictEqual(eventType, 'rename');
  assert.strictEqual(join(tmpDir, _filename), filename);
}));

writeFileSync(filename, 'foobar2');
