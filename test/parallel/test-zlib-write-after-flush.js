'use strict';
const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const gzip = zlib.createGzip();
const gunz = zlib.createUnzip();

gzip.pipe(gunz);

let output = '';
const input = 'A line of data\n';
gunz.setEncoding('utf8');
gunz.on('data', (c) => output += c);
gunz.on('end', common.mustCall(() => {
  assert.strictEqual(output, input);
  assert.strictEqual(gzip._flushFlag, zlib.Z_NO_FLUSH);
}));

// make sure that flush/write doesn't trigger an assert failure
gzip.flush();
gzip.write(input);
gzip.end();
gunz.read(0);
