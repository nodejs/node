'use strict';
const common = require('../common');

const assert = require('assert');

const spawn = require('child_process').spawn;

const badOptions = {
  input: 1234
};

assert.throws(
  () => spawn('cat', [], badOptions),
  { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' });

const stringOptions = {
  input: 'hello world'
};

const catString = spawn('cat', [], stringOptions);

catString.stdout.on('data', common.mustCall((data) => {
  assert.strictEqual(data.toString('utf8'), stringOptions.input);
}));

catString.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}));

const bufferOptions = {
  input: Buffer.from('hello world')
};

const catBuffer = spawn('cat', [], bufferOptions);

catBuffer.stdout.on('data', common.mustCall((data) => {
  assert.deepStrictEqual(data, bufferOptions.input);
}));

catBuffer.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}));

// common.getArrayBufferViews expects a buffer
// with length an multiple of 8
const msgBuf = Buffer.from('hello world'.repeat(8));
for (const arrayBufferView of common.getArrayBufferViews(msgBuf)) {
  const options = {
    input: arrayBufferView
  };

  const catArrayBufferView = spawn('cat', [], options);

  catArrayBufferView.stdout.on('data', common.mustCall((data) => {
    assert.deepStrictEqual(data, msgBuf);
  }));

  catArrayBufferView.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
}
