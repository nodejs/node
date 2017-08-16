// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');


const filepath = path.join(common.tmpDir, 'write_pos.txt');


const cb_expected = 'write open close write open close write open close ';
let cb_occurred = '';

const fileDataInitial = 'abcdefghijklmnopqrstuvwxyz';

const fileDataExpected_1 = 'abcdefghijklmnopqrstuvwxyz';
const fileDataExpected_2 = 'abcdefghij123456qrstuvwxyz';
const fileDataExpected_3 = 'abcdefghij\u2026\u2026qrstuvwxyz';


process.on('exit', function() {
  if (cb_occurred !== cb_expected) {
    console.log('  Test callback events missing or out of order:');
    console.log('    expected: %j', cb_expected);
    console.log('    occurred: %j', cb_occurred);
    assert.strictEqual(
      cb_occurred, cb_expected,
      `events missing or out of order: "${cb_occurred}" !== "${cb_expected}"`);
  }
});


common.refreshTmpDir();


function run_test_1() {
  const options = {};
  const file = fs.createWriteStream(filepath, options);
  console.log('    (debug: start         ', file.start);
  console.log('    (debug: pos           ', file.pos);

  file.on('open', function(fd) {
    cb_occurred += 'open ';
  });

  file.on('close', function() {
    cb_occurred += 'close ';
    console.log('    (debug: bytesWritten  ', file.bytesWritten);
    console.log('    (debug: start         ', file.start);
    console.log('    (debug: pos           ', file.pos);
    assert.strictEqual(file.bytesWritten, buffer.length);
    const fileData = fs.readFileSync(filepath, 'utf8');
    console.log('    (debug: file data   ', fileData);
    console.log('    (debug: expected    ', fileDataExpected_1);
    assert.strictEqual(fileData, fileDataExpected_1);

    run_test_2();
  });

  file.on('error', function(err) {
    cb_occurred += 'error ';
    console.log('    (debug: err event ', err);
    throw err;
  });

  const buffer = Buffer.from(fileDataInitial);
  file.write(buffer);
  cb_occurred += 'write ';

  file.end();
}


function run_test_2() {

  const buffer = Buffer.from('123456');

  const options = { start: 10,
                    flags: 'r+' };
  const file = fs.createWriteStream(filepath, options);
  console.log('    (debug: start         ', file.start);
  console.log('    (debug: pos           ', file.pos);

  file.on('open', function(fd) {
    cb_occurred += 'open ';
  });

  file.on('close', function() {
    cb_occurred += 'close ';
    console.log('    (debug: bytesWritten  ', file.bytesWritten);
    console.log('    (debug: start         ', file.start);
    console.log('    (debug: pos           ', file.pos);
    assert.strictEqual(file.bytesWritten, buffer.length);
    const fileData = fs.readFileSync(filepath, 'utf8');
    console.log('    (debug: file data   ', fileData);
    console.log('    (debug: expected    ', fileDataExpected_2);
    assert.strictEqual(fileData, fileDataExpected_2);

    run_test_3();
  });

  file.on('error', function(err) {
    cb_occurred += 'error ';
    console.log('    (debug: err event ', err);
    throw err;
  });

  file.write(buffer);
  cb_occurred += 'write ';

  file.end();
}


function run_test_3() {

  const data = '\u2026\u2026';    // 3 bytes * 2 = 6 bytes in UTF-8

  const options = { start: 10,
                    flags: 'r+' };
  const file = fs.createWriteStream(filepath, options);
  console.log('    (debug: start         ', file.start);
  console.log('    (debug: pos           ', file.pos);

  file.on('open', function(fd) {
    cb_occurred += 'open ';
  });

  file.on('close', function() {
    cb_occurred += 'close ';
    console.log('    (debug: bytesWritten  ', file.bytesWritten);
    console.log('    (debug: start         ', file.start);
    console.log('    (debug: pos           ', file.pos);
    assert.strictEqual(file.bytesWritten, data.length * 3);
    const fileData = fs.readFileSync(filepath, 'utf8');
    console.log('    (debug: file data   ', fileData);
    console.log('    (debug: expected    ', fileDataExpected_3);
    assert.strictEqual(fileData, fileDataExpected_3);

    run_test_4();
  });

  file.on('error', function(err) {
    cb_occurred += 'error ';
    console.log('    (debug: err event ', err);
    throw err;
  });

  file.write(data, 'utf8');
  cb_occurred += 'write ';

  file.end();
}


const run_test_4 = common.mustCall(function() {
  //  Error: start must be >= zero
  assert.throws(
    function() {
      fs.createWriteStream(filepath, { start: -5, flags: 'r+' });
    },
    /"start" must be/
  );
});

run_test_1();
