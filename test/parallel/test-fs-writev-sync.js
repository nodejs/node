'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const expected = 'ümlaut. Лорем 運務ホソモ指及 आपको करने विकास 紙読決多密所 أضف';

const getFileName = (i) => tmpdir.resolve(`writev_sync_${i}.txt`);

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
  assert.strictEqual(written, 0);

  written = fs.writevSync(fd, bufferArr, null);
  assert.strictEqual(written, expectedLength);

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
  assert.strictEqual(written, 0);

  written = fs.writevSync(fd, bufferArr);
  assert.strictEqual(written, expectedLength);

  fs.closeSync(fd);

  assert(Buffer.concat(bufferArr).equals(fs.readFileSync(filename)));
}

// fs.writevSync with empty array of buffers
{
  const filename = getFileName(3);
  const fd = fs.openSync(filename, 'w');
  const written = fs.writevSync(fd, []);
  assert.strictEqual(written, 0);
  fs.closeSync(fd);

}

/**
 * Testing with wrong input types
 */
{
  const filename = getFileName(4);
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
