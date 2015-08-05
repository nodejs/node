'use strict';
// test unzipping a file that was created with a non-node gzip lib,
// piped in as fast as possible.

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');
const path = require('path');

common.refreshTmpDir();

var gunzip = zlib.createGunzip();

const fs = require('fs');

var fixture = path.resolve(common.fixturesDir, 'person.jpg.gz');
var unzippedFixture = path.resolve(common.fixturesDir, 'person.jpg');
var outputFile = path.resolve(common.tmpDir, 'person.jpg');
var expect = fs.readFileSync(unzippedFixture);
var inp = fs.createReadStream(fixture);
var out = fs.createWriteStream(outputFile);

inp.pipe(gunzip).pipe(out);
out.on('close', function() {
  var actual = fs.readFileSync(outputFile);
  assert.equal(actual.length, expect.length, 'length should match');
  for (var i = 0, l = actual.length; i < l; i++) {
    assert.equal(actual[i], expect[i], 'byte[' + i + ']');
  }
});
