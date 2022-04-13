'use strict';
require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const path = require('path');
const stream = require('stream');
const tmpdir = require('../common/tmpdir');
const firstEncoding = 'base64';
const secondEncoding = 'latin1';

const examplePath = fixtures.path('x.txt');
const dummyPath = path.join(tmpdir.path, 'x.txt');

tmpdir.refresh();

const exampleReadStream = fs.createReadStream(examplePath, {
  encoding: firstEncoding
});

const dummyWriteStream = fs.createWriteStream(dummyPath, {
  encoding: firstEncoding
});

exampleReadStream.pipe(dummyWriteStream).on('finish', function() {
  const assertWriteStream = new stream.Writable({
    write: function(chunk, enc, next) {
      const expected = Buffer.from('xyz\n');
      assert(chunk.equals(expected));
    }
  });
  assertWriteStream.setDefaultEncoding(secondEncoding);
  fs.createReadStream(dummyPath, {
    encoding: secondEncoding
  }).pipe(assertWriteStream);
});
