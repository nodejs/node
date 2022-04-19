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

// Test for callback API.
fs.open(filepath, 'r', (_, fd) => {
  assert.throws(
    () => fs.read(fd, { offset: null, buffer: buf }, () => {}),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
  assert.throws(
    () => fs.read(fd, buf, { offset: null }, () => {}),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
  fs.close(fd, common.mustSucceed(() => {}));
});

let filehandle = null;

// Test for promise api
(async () => {
  filehandle = await fsPromises.open(filepath, 'r');
  assert.rejects(
    async () => filehandle.read(buf, { offset: null }),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
})()
.then(common.mustCall())
.finally(() => filehandle?.close());

(async () => {
  filehandle = await fsPromises.open(filepath, 'r');

  // In this test, null is interpreted as default options, not null offset.
  // 120 is the ascii code of letter x.
  const readObject = await filehandle.read(buf, null);
  assert.strictEqual(readObject.buffer[0], 120);
})()
.then(common.mustCall())
.finally(() => filehandle?.close());
