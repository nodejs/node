import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import fs from 'fs';
import assert from 'assert';

// This test ensures that "position" argument is correctly validated

const filepath = fixtures.path('x.txt');

const buffer = Buffer.from('xyz\n');
const offset = 0;
const length = buffer.byteLength;

// allowedErrors is an array of acceptable internal errors
// For example, on some platforms read syscall might return -EFBIG
async function testValid(position, allowedErrors = []) {
  return new Promise((resolve, reject) => {
    fs.open(filepath, 'r', common.mustSucceed((fd) => {
      let callCount = 3;
      const handler = common.mustCall((err) => {
        callCount--;
        if (err && !allowedErrors.includes(err.code)) {
          fs.close(fd, common.mustSucceed());
          reject(err);
        } else if (callCount === 0) {
          fs.close(fd, common.mustSucceed(resolve));
        }
      }, callCount);
      fs.read(fd, buffer, offset, length, position, handler);
      fs.read(fd, { buffer, offset, length, position }, handler);
      fs.read(fd, buffer, { offset, length, position }, handler);
    }));
  });
}

async function testInvalid(code, position) {
  return new Promise((resolve, reject) => {
    fs.open(filepath, 'r', common.mustSucceed((fd) => {
      try {
        assert.throws(
          () => fs.read(fd, buffer, offset, length, position, common.mustNotCall()),
          { code }
        );
        assert.throws(
          () => fs.read(fd, { buffer, offset, length, position }, common.mustNotCall()),
          { code }
        );
        assert.throws(
          () => fs.read(fd, buffer, { offset, length, position }, common.mustNotCall()),
          { code }
        );
        resolve();
      } catch (err) {
        reject(err);
      } finally {
        fs.close(fd, common.mustSucceed());
      }
    }));
  });
}

{
  await testValid(undefined);
  await testValid(null);
  await testValid(-1);
  await testValid(-1n);

  await testValid(0);
  await testValid(0n);
  await testValid(1);
  await testValid(1n);
  await testValid(9);
  await testValid(9n);
  await testValid(Number.MAX_SAFE_INTEGER, [ 'EFBIG' ]);

  await testValid(2n ** 63n - 1n - BigInt(length), [ 'EFBIG' ]);
  await testInvalid('ERR_OUT_OF_RANGE', 2n ** 63n);

  // TODO(LiviaMedeiros): test `2n ** 63n - BigInt(length)`

  await testInvalid('ERR_OUT_OF_RANGE', NaN);
  await testInvalid('ERR_OUT_OF_RANGE', -Infinity);
  await testInvalid('ERR_OUT_OF_RANGE', Infinity);
  await testInvalid('ERR_OUT_OF_RANGE', -0.999);
  await testInvalid('ERR_OUT_OF_RANGE', -(2n ** 64n));
  await testInvalid('ERR_OUT_OF_RANGE', Number.MAX_SAFE_INTEGER + 1);
  await testInvalid('ERR_OUT_OF_RANGE', Number.MAX_VALUE);

  for (const badTypeValue of [
    false, true, '1', Symbol(1), {}, [], () => {}, Promise.resolve(1),
  ]) {
    await testInvalid('ERR_INVALID_ARG_TYPE', badTypeValue);
  }
}
