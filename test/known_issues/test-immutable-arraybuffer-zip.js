// Flags: --js-immutable-arraybuffer
'use strict';

require('../common');
const assert = require('node:assert');
const { test } = require('node:test');
const { ZipEntry, ZipBuffer, createZipArchiveSync } = require('node:zlib');

// ZIP copies must accept immutable source bytes, including payloads retained
// from an immutable archive.
// Keep the input compressible so DEFLATE does not fall back to STORE.
const data = Buffer.from('test'.repeat(256));

function immutable(buffer) {
  return Buffer.from(buffer.buffer.sliceToImmutable(
    buffer.byteOffset, buffer.byteOffset + buffer.length));
}

function archive(method) {
  const entry = ZipEntry.createSync('file', data, { method });
  assert.strictEqual(entry.method, method === 'store' ? 0 : 8);
  return Buffer.concat([...createZipArchiveSync([entry])]);
}

for (const method of ['content', 'contentSync']) {
  test(`ZipEntry.${method} with immutable STORE input`, async () => {
    const entry = ZipEntry.createSync('file', data, { method: 'store' });
    assert.deepStrictEqual(await entry[method](), data);

    const source = immutable(data);
    const immutableEntry = ZipEntry.createSync('file', source, { method: 'store' });
    assert.deepStrictEqual(await immutableEntry[method](), data);
    assert.deepStrictEqual([...source], [...data]);
  });

  test(`ZipEntry.${method} with an immutable STORE archive`, async () => {
    const bytes = archive('store');
    using mutableZip = new ZipBuffer(bytes);
    assert.deepStrictEqual(await mutableZip.get('file')[method](), data);

    const source = immutable(bytes);
    using zip = new ZipBuffer(source);
    assert.deepStrictEqual(await zip.get('file')[method](), data);
    assert.deepStrictEqual([...source], [...bytes]);
  });
}

for (const compression of ['store', 'deflate']) {
  for (const method of ['toBuffer', 'toBufferSync']) {
    test(`ZipBuffer.${method} with an immutable ${compression} archive`, async () => {
      const bytes = archive(compression);
      using mutableZip = new ZipBuffer(bytes);
      using control = new ZipBuffer(await mutableZip[method]());
      assert.deepStrictEqual(control.get('file').contentSync(), data);

      const source = immutable(bytes);
      using zip = new ZipBuffer(source);
      using result = new ZipBuffer(await zip[method]());
      assert.deepStrictEqual(result.get('file').contentSync(), data);
      assert.deepStrictEqual([...source], [...bytes]);
    });
  }
}
