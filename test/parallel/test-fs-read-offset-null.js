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
// Reading only one character, hence buffer of one byte is enough

// Test for callback api
fs.open(filepath, 'r', common.mustSucceed((fd) => {
  fs.read(fd, { offset: null, buffer: buf },
          common.mustSucceed((bytesRead, buffer) => {
            assert.strictEqual(buffer[0], 120);
            // Test is done by making sure the first letter in buffer is
            // same as first letter in file.
            // 66 is the hex for ascii code of letter B

            fs.close(fd, common.mustSucceed(() => {}));
          }));
}));

let filehandle = null;

// Test for promise api
(async () => {
  filehandle = await fsPromises.open(filepath, 'r');
  const readObject = await filehandle.read(buf, null, buf.length);
  assert.strictEqual(readObject.buffer[0], 120);
})()
.then(common.mustCall())
.finally(async () => {
// Close the file handle if it is opened
  if (filehandle)
    await filehandle.close();
});
