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

// Test is done by making sure the first letter in buffer is
// same as first letter in file.
// 120 is the ascii code of letter x.

// Test for callback API.
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

let filehandle = null;

// Test for promise api
(async () => {
  filehandle = await fsPromises.open(filepath, 'r');
  const readObject = await filehandle.read(buf, { offset: null }, buf.length);
  assert.strictEqual(readObject.buffer[0], 120);
})()
.then(common.mustCall())
.finally(() => filehandle?.close());

(async () => {
  filehandle = await fsPromises.open(filepath, 'r');
  const readObject = await filehandle.read(buf, null);
  assert.strictEqual(readObject.buffer[0], 120);
})()
.then(common.mustCall())
.finally(() => filehandle?.close());
