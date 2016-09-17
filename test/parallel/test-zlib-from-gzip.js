'use strict';
// test unzipping a file that was created with a non-node gzip lib,
// piped in as fast as possible.

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');
const path = require('path');

common.refreshTmpDir();

const gunzip = zlib.createGunzip();

const fs = require('fs');

const fixture = path.resolve(common.fixturesDir, 'person.jpg.gz');
const unzippedFixture = path.resolve(common.fixturesDir, 'person.jpg');
const outputFile = path.resolve(common.tmpDir, 'person.jpg');
const expect = fs.readFileSync(unzippedFixture);
const inp = fs.createReadStream(fixture);
const out = fs.createWriteStream(outputFile);

inp.pipe(gunzip).pipe(out);
out.on('close', function() {
  const actual = fs.readFileSync(outputFile);
  assert.equal(actual.length, expect.length, 'length should match');
  for (var i = 0, l = actual.length; i < l; i++) {
    assert.equal(actual[i], expect[i], 'byte[' + i + ']');
  }
});
