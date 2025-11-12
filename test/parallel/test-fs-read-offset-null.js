'use strict';


// Test to assert the desired functioning of fs.read
// when {offset:null} is passed as options parameter

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const fsPromises = fs.promises;
const fixtures = require('../common/fixtures');
const filepath = fixtures.path('x.txt');

const buf = Buffer.alloc(1);
// Reading only one character, hence buffer of one byte is enough.

// Tests are done by making sure the first letter in buffer is
// same as first letter in file.
// 120 is the ascii code of letter x.

// Tests for callback API.
fs.open(filepath, 'r', common.mustSucceed((fd) => {
  fs.read(fd, { offset: null, buffer: buf },
          common.mustSucceed((bytesRead, buffer) => {
            assert.strictEqual(buffer[0], 120);
            fs.close(fd, common.mustSucceed(() => {}));
          }));
}));

fs.open(filepath, 'r', common.mustSucceed((fd) => {
  fs.read(fd, buf, { offset: null },
          common.mustSucceed((bytesRead, buffer) => {
            assert.strictEqual(buffer[0], 120);
            fs.close(fd, common.mustSucceed(() => {}));
          }));
}));

// Tests for promises api
(async () => {
  await using filehandle = await fsPromises.open(filepath, 'r');
  const readObject = await filehandle.read(buf, { offset: null });
  assert.strictEqual(readObject.buffer[0], 120);
})()
.then(common.mustCall());

// Undocumented: omitted position works the same as position === null
(async () => {
  await using filehandle = await fsPromises.open(filepath, 'r');
  const readObject = await filehandle.read(buf, null, buf.length);
  assert.strictEqual(readObject.buffer[0], 120);
})()
.then(common.mustCall());

(async () => {
  await using filehandle = await fsPromises.open(filepath, 'r');
  const readObject = await filehandle.read(buf, null, buf.length, 0);
  assert.strictEqual(readObject.buffer[0], 120);
})()
.then(common.mustCall());
