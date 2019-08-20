'use strict';

require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs').promises;
const tmpdir = require('../common/tmpdir');
const expected = 'ümlaut. Лорем 運務ホソモ指及 आपको करने विकास 紙読決多密所 أضف';
let cnt = 0;

function getFileName() {
  return path.join(tmpdir.path, `writev_promises_${++cnt}.txt`);
}

tmpdir.refresh();

(async () => {
  {
    const filename = getFileName();
    const handle = await fs.open(filename, 'w');
    const buffer = Buffer.from(expected);
    const bufferArr = [buffer, buffer];
    const expectedLength = bufferArr.length * buffer.byteLength;
    let { bytesWritten, buffers } = await handle.writev([Buffer.from('')],
                                                        null);
    assert.deepStrictEqual(bytesWritten, 0);
    assert.deepStrictEqual(buffers, [Buffer.from('')]);
    ({ bytesWritten, buffers } = await handle.writev(bufferArr, null));
    assert.deepStrictEqual(bytesWritten, expectedLength);
    assert.deepStrictEqual(buffers, bufferArr);
    assert(Buffer.concat(bufferArr).equals(await fs.readFile(filename)));
    handle.close();
  }

  // fs.promises.writev() with an array of buffers without position.
  {
    const filename = getFileName();
    const handle = await fs.open(filename, 'w');
    const buffer = Buffer.from(expected);
    const bufferArr = [buffer, buffer, buffer];
    const expectedLength = bufferArr.length * buffer.byteLength;
    let { bytesWritten, buffers } = await handle.writev([Buffer.from('')]);
    assert.deepStrictEqual(bytesWritten, 0);
    assert.deepStrictEqual(buffers, [Buffer.from('')]);
    ({ bytesWritten, buffers } = await handle.writev(bufferArr));
    assert.deepStrictEqual(bytesWritten, expectedLength);
    assert.deepStrictEqual(buffers, bufferArr);
    assert(Buffer.concat(bufferArr).equals(await fs.readFile(filename)));
    handle.close();
  }
})();
