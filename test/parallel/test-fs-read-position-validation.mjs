import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import fs from 'fs';
import assert from 'assert';
import util from 'util';

// This test ensures that "position" argument is correctly validated

const filepath = fixtures.path('x.txt');

const buffer = Buffer.from('xyz\n');
const offset = 0;
const length = buffer.byteLength;

const close = util.promisify(fs.close);
const open = util.promisify(fs.open);
const read = util.promisify(fs.read);

// allowedErrors is an array of acceptable internal errors
// For example, on some platforms read syscall might return -EFBIG
async function testValid(position, allowedErrors = []) {
  let fd;
  try {
    fd = await open(filepath, 'r');
    if (allowedErrors.length) {
      await read(fd, buffer, offset, length, position).catch((err) => {
        if (!allowedErrors.includes(err.code)) {
          assert.fail(err);
        }
      });
      await read(fd, { buffer, offset, length, position }).catch((err) => {
        if (!allowedErrors.includes(err.code)) {
          assert.fail(err);
        }
      });
    } else {
      await read(fd, buffer, offset, length, position);
      await read(fd, { buffer, offset, length, position });
    }
  } finally {
    if (fd) await close(fd);
  }
}

async function testInvalid(code, position) {
  let fd;
  try {
    fd = await open(filepath, 'r');
    await assert.rejects(
      read(fd, buffer, offset, length, position),
      { code }
    );
    await assert.rejects(
      read(fd, { buffer, offset, length, position }),
      { code }
    );
  } finally {
    if (fd) await close(fd);
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
