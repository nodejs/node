'use strict';
// Test unzipping a gzip file that contains multiple concatenated "members"

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');
const path = require('path');
const fs = require('fs');

const data = Buffer.concat([
  zlib.gzipSync('abc'),
  zlib.gzipSync('def')
]);

assert.equal(zlib.gunzipSync(data).toString(), 'abcdef');

zlib.gunzip(data, common.mustCall((err, result) => {
  assert.ifError(err);
  assert.equal(result, 'abcdef', 'result should match original string');
}));

// files that have the "right" magic bytes for starting a new gzip member
// in the middle of themselves, even if they are part of a single
// regularly compressed member
const pmmFileZlib = path.join(common.fixturesDir, 'pseudo-multimember-gzip.z');
const pmmFileGz = path.join(common.fixturesDir, 'pseudo-multimember-gzip.gz');

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
