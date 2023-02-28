'use strict';
// Flags: --expose-gc

require('../common');
const assert = require('assert');
const { ZipArchive } = require('zlib');
const fixtures = require('../common/fixtures');

const archive = fixtures.readSync('corepack.zip');
const payload = 'Hello'.repeat(1000);

function checkMemory(cb, iter) {
  const before = process.memoryUsage().rss;
  const tolerance = 8 * 1024 * 1024;

  for (let t = 0; t < iter; ++t) {
    cb();
    global.gc();
  }

  const after = process.memoryUsage().rss;

  assert.strictEqual(
    Math.max(after - before, tolerance),
    tolerance,
    new Error('Memory usage exceeded the specified allowance'),
  );
}

checkMemory(() => {
  new ZipArchive();
}, 100);

checkMemory(() => {
  new ZipArchive(archive);
}, 100);

checkMemory(() => {
  new ZipArchive(archive).digest();
}, 100);

{
  const zip = new ZipArchive();
  checkMemory(() => {
    zip.addFile('foo.txt', payload);
  }, 100);
  zip.digest();
}
