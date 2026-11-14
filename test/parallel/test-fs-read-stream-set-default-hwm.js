'use strict';

const common = require('../common');

const assert = require('node:assert');
const fs = require('node:fs');
const { setDefaultHighWaterMark } = require('node:stream');

{
  const stream = fs.createReadStream(__filename);
  assert.strictEqual(stream.readableHighWaterMark, 64 * 1024);
  stream.destroy();
}

setDefaultHighWaterMark(false, 1024 * 1024);

{
  const stream = fs.createReadStream(__filename);
  assert.strictEqual(stream.readableHighWaterMark, 1024 * 1024);
  stream.destroy();
}

{
  const stream = fs.createReadStream(__filename, { highWaterMark: 1024 });
  assert.strictEqual(stream.readableHighWaterMark, 1024);
  stream.destroy();
}

fs.promises.open(__filename).then(async (fh) => {
  const stream = fh.createReadStream({ autoClose: false });
  assert.strictEqual(stream.readableHighWaterMark, 1024 * 1024);
  stream.destroy();
  await fh.close();
}).then(common.mustCall());
