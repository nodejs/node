// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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
