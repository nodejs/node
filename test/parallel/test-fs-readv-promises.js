'use strict';

require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs').promises;
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const expected = 'ümlaut. Лорем 運務ホソモ指及 आपको करने विकास 紙読決多密所 أضف';
const exptectedBuff = Buffer.from(expected);

let cnt = 0;
function getFileName() {
  return path.join(tmpdir.path, `readv_promises_${++cnt}.txt`);
}

const allocateEmptyBuffers = (combinedLength) => {
  const bufferArr = [];
  // Allocate two buffers, each half the size of exptectedBuff
  bufferArr[0] = Buffer.alloc(Math.floor(combinedLength / 2)),
  bufferArr[1] = Buffer.alloc(combinedLength - bufferArr[0].length);

  return bufferArr;
};

(async () => {
  {
    const filename = getFileName();
    await fs.writeFile(filename, exptectedBuff);
    const handle = await fs.open(filename, 'r');
    // const buffer = Buffer.from(expected);
    const bufferArr = allocateEmptyBuffers(exptectedBuff.length);
    const expectedLength = exptectedBuff.length;

    let { bytesRead, buffers } = await handle.readv([Buffer.from('')],
                                                    null);
    assert.deepStrictEqual(bytesRead, 0);
    assert.deepStrictEqual(buffers, [Buffer.from('')]);

    ({ bytesRead, buffers } = await handle.readv(bufferArr, null));
    assert.deepStrictEqual(bytesRead, expectedLength);
    assert.deepStrictEqual(buffers, bufferArr);
    assert(Buffer.concat(bufferArr).equals(await fs.readFile(filename)));
    handle.close();
  }

  {
    const filename = getFileName();
    await fs.writeFile(filename, exptectedBuff);
    const handle = await fs.open(filename, 'r');
    // const buffer = Buffer.from(expected);
    const bufferArr = allocateEmptyBuffers(exptectedBuff.length);
    const expectedLength = exptectedBuff.length;

    let { bytesRead, buffers } = await handle.readv([Buffer.from('')]);
    assert.deepStrictEqual(bytesRead, 0);
    assert.deepStrictEqual(buffers, [Buffer.from('')]);

    ({ bytesRead, buffers } = await handle.readv(bufferArr));
    assert.deepStrictEqual(bytesRead, expectedLength);
    assert.deepStrictEqual(buffers, bufferArr);
    assert(Buffer.concat(bufferArr).equals(await fs.readFile(filename)));
    handle.close();
  }
})();
