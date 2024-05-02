'use strict';

const common = require('../common');

// This test ensures that fs.write accepts "named parameters" object
// and doesn't interpret objects as strings

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const util = require('util');

tmpdir.refresh();

const destInvalid = tmpdir.resolve('rwopt_invalid');
const buffer = Buffer.from('zyx');

function testInvalidCb(fd, expectedCode, buffer, options, callback) {
  assert.throws(
    () => fs.write(fd, buffer, common.mustNotMutateObjectDeep(options), common.mustNotCall()),
    { code: expectedCode }
  );
  callback(0);
}

function testValidCb(buffer, options, index, callback) {
  options = common.mustNotMutateObjectDeep(options);
  const length = options?.length;
  const offset = options?.offset;
  const dest = tmpdir.resolve(`rwopt_valid_${index}`);
  fs.open(dest, 'w', common.mustSucceed((fd) => {
    fs.write(fd, buffer, options, common.mustSucceed((bytesWritten, bufferWritten) => {
      const writeBufCopy = Uint8Array.prototype.slice.call(bufferWritten);
      fs.close(fd, common.mustSucceed(() => {
        fs.open(dest, 'r', common.mustSucceed((fd) => {
          fs.read(fd, buffer, options, common.mustSucceed((bytesRead, bufferRead) => {
            const readBufCopy = Uint8Array.prototype.slice.call(bufferRead);

            assert.ok(bytesWritten >= bytesRead);
            if (length !== undefined && length !== null) {
              assert.strictEqual(bytesWritten, length);
              assert.strictEqual(bytesRead, length);
            }
            if (offset === undefined || offset === 0) {
              assert.deepStrictEqual(writeBufCopy, readBufCopy);
            }
            assert.deepStrictEqual(bufferWritten, bufferRead);
            fs.close(fd, common.mustSucceed(callback));
          }));
        }));
      }));
    }));
  }));
}

// Promisify to reduce flakiness
const testInvalid = util.promisify(testInvalidCb);
const testValid = util.promisify(testValidCb);

async function runTests(fd) {
  // Test if first argument is not wrongly interpreted as ArrayBufferView|string
  for (const badBuffer of [
    undefined, null, true, 42, 42n, Symbol('42'), NaN, [], () => {},
    Promise.resolve(new Uint8Array(1)),
    common.mustNotCall(),
    common.mustNotMutateObjectDeep({}),
    {},
    { buffer: 'amNotParam' },
    { string: 'amNotParam' },
    { buffer: new Uint8Array(1).buffer },
    new Date(),
    new String('notPrimitive'),
    { [Symbol.toPrimitive]: (hint) => 'amObject' },
    { toString() { return 'amObject'; } },
  ]) {
    await testInvalid(fd, 'ERR_INVALID_ARG_TYPE', badBuffer, {});
  }

  // First argument (buffer or string) is mandatory
  await testInvalid(fd, 'ERR_INVALID_ARG_TYPE', undefined, undefined);

  // Various invalid options
  await testInvalid(fd, 'ERR_OUT_OF_RANGE', buffer, { length: 5 });
  await testInvalid(fd, 'ERR_OUT_OF_RANGE', buffer, { offset: 5 });
  await testInvalid(fd, 'ERR_OUT_OF_RANGE', buffer, { length: 1, offset: 3 });
  await testInvalid(fd, 'ERR_OUT_OF_RANGE', buffer, { length: -1 });
  await testInvalid(fd, 'ERR_OUT_OF_RANGE', buffer, { offset: -1 });
  await testInvalid(fd, 'ERR_INVALID_ARG_TYPE', buffer, { offset: false });
  await testInvalid(fd, 'ERR_INVALID_ARG_TYPE', buffer, { offset: true });
  await testInvalid(fd, 'ERR_INVALID_ARG_TYPE', buffer, true);
  await testInvalid(fd, 'ERR_INVALID_ARG_TYPE', buffer, '42');
  await testInvalid(fd, 'ERR_INVALID_ARG_TYPE', buffer, Symbol('42'));

  // Test compatibility with fs.read counterpart
  for (const [ index, options ] of [
    null,
    {},
    { length: 1 },
    { position: 5 },
    { length: 1, position: 5 },
    { length: 1, position: -1, offset: 2 },
    { length: null },
    { position: null },
    { offset: 1 },
  ].entries()) {
    await testValid(buffer, options, index);
  }
}

fs.open(destInvalid, 'w+', common.mustSucceed(async (fd) => {
  runTests(fd).then(common.mustCall(() => fs.close(fd, common.mustSucceed())));
}));
