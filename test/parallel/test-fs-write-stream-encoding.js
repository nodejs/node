'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const stream = require('stream');
const firstEncoding = 'base64';
const secondEncoding = 'binary';

const example = path.join(common.fixturesDir, 'x.txt');
const dummy = path.join(common.tmpDir, '/x.txt');
const exampleReadStream = fs.createReadStream(example, firstEncoding);
const dummyWriteStream = fs.createWriteStream(dummy, firstEncoding);
exampleReadStream.pipe(dummyWriteStream).on('finish', function(){
  const assertWriteStream = new stream.Writable({
    write: function(chunk, enc, next) {
      const expected = new Buffer('xyz\n');
      assert(chunk.equals(expected));
    }
  });
  assertWriteStream.setDefaultEncoding(secondEncoding);
  fs.createReadStream(dummy, secondEncoding).pipe(assertWriteStream);
});
