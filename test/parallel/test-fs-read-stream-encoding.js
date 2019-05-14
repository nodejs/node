'use strict';
require('../common');
const assert = require('assert');
const fs = require('fs');
const stream = require('stream');
const fixtures = require('../common/fixtures');
const encoding = 'base64';

const example = fixtures.path('x.txt');
const assertStream = new stream.Writable({
  write: function(chunk, enc, next) {
    const expected = Buffer.from('xyz');
    assert(chunk.equals(expected));
  }
});
assertStream.setDefaultEncoding(encoding);
fs.createReadStream(example, encoding).pipe(assertStream);
