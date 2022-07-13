'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const assert = require('assert');
const filepath = fixtures.path('x.txt');

const expected = Buffer.from('xyz\n');
const defaultBufferAsync = Buffer.alloc(16384);
const bufferAsOption = Buffer.allocUnsafe(expected.byteLength);

function testValid(message, ...options) {
  const paramsMsg = `${message} (as params)`;
  const paramsFilehandle = fs.openSync(filepath, 'r');
  fs.read(paramsFilehandle, ...options, common.mustSucceed((bytesRead, buffer) => {
    assert.strictEqual(bytesRead, expected.byteLength, paramsMsg);
    assert.deepStrictEqual(defaultBufferAsync.byteLength, buffer.byteLength, paramsMsg);
    fs.closeSync(paramsFilehandle);
  }));

  const optionsMsg = `${message} (as options)`;
  const optionsFilehandle = fs.openSync(filepath, 'r');
  fs.read(optionsFilehandle, bufferAsOption, ...options, common.mustSucceed((bytesRead, buffer) => {
    assert.strictEqual(bytesRead, expected.byteLength, optionsMsg);
    assert.deepStrictEqual(bufferAsOption.byteLength, buffer.byteLength, optionsMsg);
    fs.closeSync(optionsFilehandle);
  }));
}

testValid('Not passing in any object');
testValid('Passing in a null', null);
testValid('Passing in an empty object', {});
testValid('Passing in an object', {
  offset: 0,
  length: bufferAsOption.byteLength,
  position: 0,
});
