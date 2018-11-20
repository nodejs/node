'use strict';
const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const data = Buffer.concat([
  zlib.gzipSync('abc'),
  zlib.gzipSync('def')
]);

const resultBuffers = [];

const unzip = zlib.createUnzip()
  .on('error', (err) => {
    assert.ifError(err);
  })
  .on('data', (data) => resultBuffers.push(data))
  .on('finish', common.mustCall(() => {
    assert.deepStrictEqual(Buffer.concat(resultBuffers).toString(), 'abcdef',
                           `'${Buffer.concat(resultBuffers).toString()}' ` +
                           'should match \'abcdef\' after ' +
                           'zipping and unzipping');
  }));

for (let i = 0; i < data.length; i++) {
  // Write each single byte individually.
  unzip.write(Buffer.from([data[i]]));
}

unzip.end();
