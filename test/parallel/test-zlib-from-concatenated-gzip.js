'use strict';
// Test unzipping a gzip file that contains multiple concatenated "members"

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');
const fs = require('fs');
const fixtures = require('../common/fixtures');

const abc = 'abc';
const def = 'def';

const abcEncoded = zlib.gzipSync(abc);
const defEncoded = zlib.gzipSync(def);

const data = Buffer.concat([
  abcEncoded,
  defEncoded,
]);

assert.strictEqual(zlib.gunzipSync(data).toString(), (abc + def));

zlib.gunzip(data, common.mustSucceed((result) => {
  assert.strictEqual(result.toString(), (abc + def));
}));

zlib.unzip(data, common.mustSucceed((result) => {
  assert.strictEqual(result.toString(), (abc + def));
}));

// Multi-member support does not apply to zlib inflate/deflate.
zlib.unzip(Buffer.concat([
  zlib.deflateSync('abc'),
  zlib.deflateSync('def'),
]), common.mustSucceed((result) => {
  assert.strictEqual(result.toString(), abc);
}));

// Files that have the "right" magic bytes for starting a new gzip member
// in the middle of themselves, even if they are part of a single
// regularly compressed member
const pmmFileZlib = fixtures.path('pseudo-multimember-gzip.z');
const pmmFileGz = fixtures.path('pseudo-multimember-gzip.gz');

const pmmExpected = zlib.inflateSync(fs.readFileSync(pmmFileZlib));
const pmmResultBuffers = [];

fs.createReadStream(pmmFileGz)
  .pipe(zlib.createGunzip())
  .on('error', (err) => {
    assert.ifError(err);
  })
  .on('data', (data) => pmmResultBuffers.push(data))
  .on('finish', common.mustCall(() => {
    // Result should match original random garbage
    assert.deepStrictEqual(Buffer.concat(pmmResultBuffers), pmmExpected);
  }));

// Test that the next gzip member can wrap around the input buffer boundary
[0, 1, 2, 3, 4, defEncoded.length].forEach((offset) => {
  const resultBuffers = [];

  const unzip = zlib.createGunzip()
    .on('error', (err) => {
      assert.ifError(err);
    })
    .on('data', (data) => resultBuffers.push(data))
    .on('finish', common.mustCall(() => {
      assert.strictEqual(
        Buffer.concat(resultBuffers).toString(),
        'abcdef',
        `result should match original input (offset = ${offset})`
      );
    }));

  // First write: write "abc" + the first bytes of "def"
  unzip.write(Buffer.concat([
    abcEncoded, defEncoded.slice(0, offset),
  ]));

  // Write remaining bytes of "def"
  unzip.end(defEncoded.slice(offset));
});
