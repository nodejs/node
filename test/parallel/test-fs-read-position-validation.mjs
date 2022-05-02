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
  let fdSync;
  try {
    fdSync = fs.openSync(filepath, 'r');
    fs.readSync(fdSync, buffer, offset, length, position);
    fs.readSync(fdSync, buffer, { offset, length, position });
  } catch (err) {
    if (!allowedErrors.includes(err.code)) {
      assert.fail(err);
    }
  } finally {
    if (fdSync) fs.closeSync(fdSync);
  }

  fs.open(filepath, 'r', common.mustSucceed((fd) => {
    try {
      if (allowedErrors.length) {
        fs.read(fd, buffer, offset, length, position, common.mustCall((err, ...results) => {
          if (err && !allowedErrors.includes(err.code)) {
            assert.fail(err);
          }
        }));
        fs.read(fd, { buffer, offset, length, position }, common.mustCall((err, ...results) => {
          if (err && !allowedErrors.includes(err.code)) {
            assert.fail(err);
          }
        }));
      } else {
        fs.read(fd, buffer, offset, length, position, common.mustSucceed());
        fs.read(fd, { buffer, offset, length, position }, common.mustSucceed());
      }
    } finally {
      fs.close(fd, common.mustSucceed());
    }
  }));
}

async function testInvalid(code, position, internalCatch = false) {
  let fdSync;
  try {
    fdSync = fs.openSync(filepath, 'r');
    assert.throws(
      () => fs.readSync(fdSync, buffer, offset, length, position),
      { code }
    );
    assert.throws(
      () => fs.readSync(fdSync, buffer, { offset, length, position }),
      { code }
    );
  } finally {
    if (fdSync) fs.closeSync(fdSync);
  }

  // Set this flag for catching errors via first argument of callback function
  if (internalCatch) {
    fs.open(filepath, 'r', common.mustSucceed((fd) => {
      try {
        fs.read(fd, buffer, offset, length, position, (err, ...results) => {
          assert.strictEqual(err.code, code);
        });
        fs.read(fd, { buffer, offset, length, position }, (err, ...results) => {
          assert.strictEqual(err.code, code);
        });
      } finally {
        fs.close(fd, common.mustSucceed());
      }
    }));
  } else {
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
      } finally {
        fs.close(fd, common.mustSucceed());
      }
    }));
  }
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
