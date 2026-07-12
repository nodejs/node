'use strict';

const common = require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');

const data = Buffer.concat([
  zlib.gzipSync('abc'),
  zlib.gzipSync('def'),
]);

const resultBuffers = [];

const unzip = zlib.createUnzip()
  .on('error', common.mustNotCall())
  .on('data', (data) => resultBuffers.push(data))
  .on('finish', common.mustCall(() => {
    const unzipped = Buffer.concat(resultBuffers).toString();
    assert.strictEqual(unzipped, 'abcdef',
                       `'${unzipped}' should match 'abcdef' after zipping ` +
      'and unzipping');
  }));

for (let i = 0; i < data.length; i++) {
  // Write each single byte individually.
  unzip.write(Buffer.from([data[i]]));
}

unzip.end();
