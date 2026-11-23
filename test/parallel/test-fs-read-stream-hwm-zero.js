'use strict';

const common = require('../common');

const assert = require('node:assert');
const fs = require('node:fs');
const { setDefaultHighWaterMark } = require('node:stream');

const expected = fs.readFileSync(__filename);

function readAll(stream, check) {
  const chunks = [];
  stream.on('data', (chunk) => chunks.push(chunk));
  stream.on('end', () => check(Buffer.concat(chunks)));
}

{
  const stream = fs.createReadStream(__filename, { highWaterMark: 0 });
  assert.strictEqual(stream.readableHighWaterMark, 0);
  readAll(stream, common.mustCall((data) => {
    assert.deepStrictEqual(data, expected);
  }));
}

{
  const stream = fs.createReadStream(__filename, {
    highWaterMark: 0,
    start: 10,
    end: 20,
  });
  readAll(stream, common.mustCall((data) => {
    assert.deepStrictEqual(data, expected.subarray(10, 21));
  }));
}

fs.promises.open(__filename).then(common.mustCall((fh) => {
  const stream = fh.createReadStream({ highWaterMark: 0 });
  readAll(stream, common.mustCall((data) => {
    assert.deepStrictEqual(data, expected);
  }));
}));

setDefaultHighWaterMark(false, 0);

{
  const stream = fs.createReadStream(__filename);
  assert.strictEqual(stream.readableHighWaterMark, 0);
  readAll(stream, common.mustCall((data) => {
    assert.deepStrictEqual(data, expected);
  }));
}
