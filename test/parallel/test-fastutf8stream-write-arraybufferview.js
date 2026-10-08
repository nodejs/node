'use strict';

// In 'buffer' content mode, Utf8Stream#write() used to only accept Buffer
// instances. This verifies it also accepts other ArrayBufferView types
// (TypedArrays, DataView), that byte length (not element count) is used
// when the view has multiple bytes per element, that byteOffset-based
// subviews only write the bytes they cover, and that non-ArrayBufferView
// input is still rejected.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const {
  readFile,
  Utf8Stream,
} = require('node:fs');
const { join } = require('node:path');

tmpdir.refresh();
let fileCounter = 0;

function getTempFile() {
  return join(tmpdir.path, `fastutf8stream-abv-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

function writeAndVerify(sync, data, expected) {
  const dest = getTempFile();
  const stream = new Utf8Stream({ dest, sync, contentMode: 'buffer' });

  stream.on('ready', common.mustCall(() => {
    assert.ok(stream.write(data));
    stream.end();

    stream.on('finish', common.mustCall(() => {
      readFile(dest, common.mustSucceed((buf) => {
        assert.deepStrictEqual(buf, expected);
      }));
    }));
  }));
}

for (const sync of [false, true]) {
  {
    // A plain Uint8Array (not a Buffer instance) must be accepted, and
    // written byte-for-byte.
    const view = new Uint8Array([0x68, 0x69, 0x0a]); // "hi\n"
    writeAndVerify(sync, view, Buffer.from(view));
  }

  {
    // A DataView must be accepted.
    const ab = new ArrayBuffer(4);
    new DataView(ab).setUint32(0, 0x61626364); // "abcd"
    const view = new DataView(ab);
    writeAndVerify(sync, view, Buffer.from(ab));
  }

  {
    // Float64Array: each element is 8 bytes, so `.length` (element count)
    // must not be confused with `.byteLength` (actual byte count). If the
    // implementation used `.length` when accumulating/merging, the output
    // would be truncated to a fraction of the real byte size.
    const view = new Float64Array([1.5, -2.25, 3]);
    writeAndVerify(sync, view, Buffer.from(view.buffer, view.byteOffset, view.byteLength));
  }

  {
    // A view with a non-zero byteOffset over a shared, larger ArrayBuffer
    // must only write the bytes it covers, not the whole backing buffer.
    const ab = new ArrayBuffer(8);
    const full = new Uint8Array(ab);
    full.set([0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22]);
    const view = new Uint8Array(ab, 2, 3); // [0xcc, 0xdd, 0xee]
    writeAndVerify(sync, view, Buffer.from([0xcc, 0xdd, 0xee]));
  }

  {
    // Non-ArrayBufferView input must still be rejected in 'buffer' mode.
    const dest = getTempFile();
    const stream = new Utf8Stream({ dest, sync, contentMode: 'buffer' });

    stream.on('ready', common.mustCall(() => {
      assert.throws(() => {
        stream.write('not a buffer');
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      });
      assert.throws(() => {
        stream.write([1, 2, 3]);
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      });
      stream.end();
    }));
  }
}
