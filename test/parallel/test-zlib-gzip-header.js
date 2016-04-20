'use strict';
var common = require('../common');
var assert = require('assert');
var zlib = require('zlib');

const helloWorldIndexed = (n) => zlib.gzipSync(`Hello, World ${n}!\n`, {
  gzipHeader: {
    name: `hw-${n}.txt`
  }
});

// No header events when not requested.
const unzipWithoutHeaders = zlib.createUnzip();
unzipWithoutHeaders.on('header', () => {
  assert.fail(null, null, 'Unexpected "header" event');
});
unzipWithoutHeaders.on('finish', common.mustCall(() => {}));
unzipWithoutHeaders.write(helloWorldIndexed(1));
unzipWithoutHeaders.end(helloWorldIndexed(2));

// Three header events for three separate files, multiple chunks style.
const unzip = zlib.createUnzip({ gzipHeader: true });
const seenFilenames = [];
const expectedFilenames = ['hw-1.txt', 'hw-2.txt', 'hw-3.txt'];
unzip.on('header', common.mustCall((header) => {
  assert.ok(Buffer.isBuffer(header.name));

  seenFilenames.push(header.name.toString());
}, 3));
unzip.write(helloWorldIndexed(1));
unzip.write(helloWorldIndexed(2));
unzip.end(helloWorldIndexed(3));

process.on('exit', () => {
  assert.deepStrictEqual(seenFilenames, expectedFilenames);
});

// Three header events for three separate files, multiple chunks style.
const unzipSingleChunk = zlib.createUnzip({ gzipHeader: true });
const seenFilenamesSingleChunk = [];
unzipSingleChunk.on('header', common.mustCall((header) => {
  assert.ok(Buffer.isBuffer(header.name));

  seenFilenamesSingleChunk.push(header.name.toString());
}, 3));
unzipSingleChunk.end(Buffer.concat([
  helloWorldIndexed(1),
  helloWorldIndexed(2),
  helloWorldIndexed(3)
]));

process.on('exit', () => {
  assert.deepStrictEqual(seenFilenamesSingleChunk, expectedFilenames);
});

// Test that various types of header fields are passed through the data stream.
const testHeader = {
  text: true,
  time: 1460335349,
  os: 42,
  hcrc: true,
  extra: [
    { id: Buffer.from('AA'), content: Buffer.from('foobar') },
    { id: Buffer.from('AB'), content: Buffer.from('barbaz') },
  ],
  name: Buffer.from('original-filename'),
  comment: Buffer.from('Some file comment')
};

const unzipCheckHeader = zlib.createGunzip({ gzipHeader: true });
const gzipWithHeader = zlib.createGzip({ gzipHeader: testHeader });

unzipCheckHeader.on('header', common.mustCall((header) => {
  // The `.extraRaw` field is generated and duplicates the information from
  // `.extra`.
  assert.ok(Buffer.isBuffer(header.extraRaw));
  delete header.extraRaw;

  assert.deepStrictEqual(header, testHeader);
}));

gzipWithHeader.pipe(unzipCheckHeader);
gzipWithHeader.end('some data');

// Some argument type checks.
assert.throws(() => {
  zlib.gzipSync('foobar', {
    gzipHeader: {
      name: 42
    }
  });
}, /Cannot convert name to zero-terminated string/);

assert.throws(() => {
  zlib.gzipSync('foobar', {
    gzipHeader: {
      comment: 42
    }
  });
}, /Cannot convert comment to zero-terminated string/);

assert.throws(() => {
  zlib.gzipSync('foobar', {
    gzipHeader: {
      text: 'Yes'
    }
  });
}, /Invalid gzip header text flag:/);

assert.throws(() => {
  zlib.gzipSync('foobar', {
    gzipHeader: {
      time: 'Yes'
    }
  });
}, /Invalid gzip header time field:/);

assert.throws(() => {
  zlib.gzipSync('foobar', {
    gzipHeader: 42
  });
}, /When encoding gzipHeader needs to be object if set/);

// There is no gzip header for non-gzip input.
const unzipCheckDeflate = zlib.createUnzip({ gzipHeader: true });
const deflateWithHeader = zlib.createDeflate();

unzipCheckDeflate.on('header', (header) => {
  assert.fail(null, null, 'There should be no gzip header for non-gzip data.');
});

deflateWithHeader.pipe(unzipCheckDeflate);
deflateWithHeader.end('some data');
