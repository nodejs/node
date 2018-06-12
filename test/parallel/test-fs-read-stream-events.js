'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

const encodings = ['utf8', 'base64', 'ascii', 'binary', 'hex', 'utf16le'];

function test(encoding) {

  const file = path.join(tmpdir.path, `read-stream-events-${encoding}.txt`);

  const data = '1234';
  fs.writeFileSync(file, data);

  const rs = fs.createReadStream(file, {
    encoding,
    highWaterMark: data.length - 1
  });

  let chunks = '';

  rs.on('data', common.mustCall(function(data) {
    chunks += Buffer.from(data, encoding);
  }, 2));

  rs.on('end', common.mustCall(function() {
    assert.strictEqual(chunks, data);
  }));
}

for (const encoding of encodings)
  test(encoding);
