'use strict';
// Test unzipping a file that was created with a non-node zstd lib,
// piped in as fast as possible.
//
// The compressed fixture was created using the reference CLI:
// $ zstd -19 test/fixtures/person.jpg -o test/fixtures/person.jpg.zst

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');
const fixtures = require('../common/fixtures');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const decompress = new zlib.ZstdDecompress();

const fs = require('fs');

const fixture = fixtures.path('person.jpg.zst');
const unzippedFixture = fixtures.path('person.jpg');
const outputFile = tmpdir.resolve('person.jpg');
const expect = fs.readFileSync(unzippedFixture);
const inp = fs.createReadStream(fixture);
const out = fs.createWriteStream(outputFile);

inp.pipe(decompress).pipe(out);
out.on('close', common.mustCall(() => {
  const actual = fs.readFileSync(outputFile);
  assert.strictEqual(actual.length, expect.length);
  for (let i = 0, l = actual.length; i < l; i++) {
    assert.strictEqual(actual[i], expect[i], `byte[${i}]`);
  }
}));
