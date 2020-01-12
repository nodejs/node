'use strict';

require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const expected = 'ümlaut. Лорем 運務ホソモ指及 आपको करने विकास 紙読決多密所 أضف';

const getFileName = (i) => path.join(tmpdir.path, `writev_sync_${i}.txt`);

/**
 * Testing with a array of buffers input
 */

// fs.writevSync with array of buffers with all parameters
{
  const filename = getFileName(1);
  const fd = fs.openSync(filename, 'w');

  const buffer = Buffer.from(expected);
  const bufferArr = [buffer, buffer];
  const expectedLength = bufferArr.length * buffer.byteLength;

  let written = fs.writevSync(fd, [Buffer.from('')], null);
  assert.deepStrictEqual(written, 0);

  written = fs.writevSync(fd, bufferArr, null);
  assert.deepStrictEqual(written, expectedLength);

  fs.closeSync(fd);

  assert(Buffer.concat(bufferArr).equals(fs.readFileSync(filename)));
}

// fs.writevSync with array of buffers without position
{
  const filename = getFileName(2);
  const fd = fs.openSync(filename, 'w');

  const buffer = Buffer.from(expected);
  const bufferArr = [buffer, buffer, buffer];
  const expectedLength = bufferArr.length * buffer.byteLength;

  let written = fs.writevSync(fd, [Buffer.from('')]);
  assert.deepStrictEqual(written, 0);

  written = fs.writevSync(fd, bufferArr);
  assert.deepStrictEqual(written, expectedLength);

  fs.closeSync(fd);

  assert(Buffer.concat(bufferArr).equals(fs.readFileSync(filename)));
}

/**
 * Testing with wrong input types
 */
{
  const filename = getFileName(3);
  const fd = fs.openSync(filename, 'w');

  [false, 'test', {}, [{}], ['sdf'], null, undefined].forEach((i) => {
    assert.throws(
      () => fs.writevSync(fd, i, null), {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError'
      }
    );
  });

  fs.closeSync(fd);
}

// fs.writevSync with wrong fd types
[false, 'test', {}, [{}], null, undefined].forEach((i) => {
  assert.throws(
    () => fs.writevSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
});
