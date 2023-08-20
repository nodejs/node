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
// For example, on some platforms read syscall might return -EFBIG or -EOVERFLOW
function testValid(position, allowedErrors = []) {
  let fdSync;
  try {
    fdSync = fs.openSync(filepath, 'r');
    fs.readSync(fdSync, buffer, offset, length, position);
    fs.readSync(fdSync, buffer, common.mustNotMutateObjectDeep({ offset, length, position }));
  } catch (err) {
    if (!allowedErrors.includes(err.code)) {
      assert.fail(err);
    }
  } finally {
    if (fdSync) fs.closeSync(fdSync);
  }
}

function testInvalid(code, position, internalCatch = false) {
  let fdSync;
  try {
    fdSync = fs.openSync(filepath, 'r');
    assert.throws(
      () => fs.readSync(fdSync, buffer, offset, length, position),
      { code }
    );
    assert.throws(
      () => fs.readSync(fdSync, buffer, common.mustNotMutateObjectDeep({ offset, length, position })),
      { code }
    );
  } finally {
    if (fdSync) fs.closeSync(fdSync);
  }
}

{
  testValid(undefined);
  testValid(null);
  testValid(-1);
  testValid(-1n);

  testValid(0);
  testValid(0n);
  testValid(1);
  testValid(1n);
  testValid(9);
  testValid(9n);
  testValid(Number.MAX_SAFE_INTEGER, [ 'EFBIG', 'EOVERFLOW' ]);

  testValid(2n ** 63n - 1n - BigInt(length), [ 'EFBIG', 'EOVERFLOW' ]);
  testInvalid('ERR_OUT_OF_RANGE', 2n ** 63n);

  // TODO(LiviaMedeiros): test `2n ** 63n - BigInt(length)`

  testInvalid('ERR_OUT_OF_RANGE', NaN);
  testInvalid('ERR_OUT_OF_RANGE', -Infinity);
  testInvalid('ERR_OUT_OF_RANGE', Infinity);
  testInvalid('ERR_OUT_OF_RANGE', -0.999);
  testInvalid('ERR_OUT_OF_RANGE', -(2n ** 64n));
  testInvalid('ERR_OUT_OF_RANGE', Number.MAX_SAFE_INTEGER + 1);
  testInvalid('ERR_OUT_OF_RANGE', Number.MAX_VALUE);

  for (const badTypeValue of [
    false, true, '1', Symbol(1), {}, [], () => {}, Promise.resolve(1),
  ]) {
    testInvalid('ERR_INVALID_ARG_TYPE', badTypeValue);
  }
}
