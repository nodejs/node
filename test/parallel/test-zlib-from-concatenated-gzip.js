'use strict';
// Test unzipping a gzip file that contains multiple concatenated "members"

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');
const fs = require('fs');
const fixtures = require('../common/fixtures');

const abcEncoded = zlib.gzipSync('abc');
const defEncoded = zlib.gzipSync('def');

const data = Buffer.concat([
  abcEncoded,
  defEncoded
]);

assert.strictEqual(zlib.gunzipSync(data).toString(), 'abcdef');

zlib.gunzip(data, common.mustCall((err, result) => {
  assert.ifError(err);
  assert.strictEqual(result.toString(), 'abcdef',
                     'result should match original string');
}));

zlib.unzip(data, common.mustCall((err, result) => {
  assert.ifError(err);
  assert.strictEqual(result.toString(), 'abcdef',
                     'result should match original string');
}));

// Multi-member support does not apply to zlib inflate/deflate.
zlib.unzip(Buffer.concat([
  zlib.deflateSync('abc'),
  zlib.deflateSync('def')
]), common.mustCall((err, result) => {
  assert.ifError(err);
  assert.strictEqual(result.toString(), 'abc',
                     'result should match contents of first "member"');
}));

// files that have the "right" magic bytes for starting a new gzip member
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
    assert.deepStrictEqual(Buffer.concat(pmmResultBuffers), pmmExpected,
                           'result should match original random garbage');
  }));

// test that the next gzip member can wrap around the input buffer boundary
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

  // first write: write "abc" + the first bytes of "def"
  unzip.write(Buffer.concat([
    abcEncoded, defEncoded.slice(0, offset)
  ]));

  // write remaining bytes of "def"
  unzip.end(defEncoded.slice(offset));
});
