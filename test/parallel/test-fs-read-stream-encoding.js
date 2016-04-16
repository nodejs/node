'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const stream = require('stream');
const encoding = 'base64';

const example = path.join(common.fixturesDir, 'x.txt');
const assertStream = new stream.Writable({
  write: function(chunk, enc, next) {
    const expected = Buffer.from('xyz');
    assert(chunk.equals(expected));
  }
});
assertStream.setDefaultEncoding(encoding);
fs.createReadStream(example, encoding).pipe(assertStream);
