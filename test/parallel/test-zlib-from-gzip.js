'use strict';
// test unzipping a file that was created with a non-node gzip lib,
// piped in as fast as possible.

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');
const path = require('path');
const fixtures = require('../common/fixtures');

common.refreshTmpDir();

const gunzip = zlib.createGunzip();

const fs = require('fs');

const fixture = fixtures.path('person.jpg.gz');
const unzippedFixture = fixtures.path('person.jpg');
const outputFile = path.resolve(common.tmpDir, 'person.jpg');
const expect = fs.readFileSync(unzippedFixture);
const inp = fs.createReadStream(fixture);
const out = fs.createWriteStream(outputFile);

inp.pipe(gunzip).pipe(out);
out.on('close', common.mustCall(() => {
  const actual = fs.readFileSync(outputFile);
  assert.strictEqual(actual.length, expect.length);
  for (let i = 0, l = actual.length; i < l; i++) {
    assert.strictEqual(actual[i], expect[i], `byte[${i}]`);
  }
}));
