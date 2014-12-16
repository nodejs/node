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

var common = require('../common');
var assert = require('assert');

var path = require('path');
var fs = require('fs');
var util = require('util');


var filepath = path.join(common.tmpDir, 'write.txt');
var file;

var EXPECTED = '012345678910';

var cb_expected = 'write open drain write drain close error ';
var cb_occurred = '';

var countDrains = 0;


process.on('exit', function() {
  removeTestFile();
  if (cb_occurred !== cb_expected) {
    console.log('  Test callback events missing or out of order:');
    console.log('    expected: %j', cb_expected);
    console.log('    occurred: %j', cb_occurred);
    assert.strictEqual(cb_occurred, cb_expected,
        'events missing or out of order: "' +
        cb_occurred + '" !== "' + cb_expected + '"');
  } else {
    console.log('ok');
  }
});

function removeTestFile() {
  try {
    fs.unlinkSync(filepath);
  } catch (e) {}
}


removeTestFile();

// drain at 0, return false at 10.
file = fs.createWriteStream(filepath, {
  highWaterMark: 11
});

file.on('open', function(fd) {
  console.error('open');
  cb_occurred += 'open ';
  assert.equal(typeof fd, 'number');
});

file.on('drain', function() {
  console.error('drain');
  cb_occurred += 'drain ';
  ++countDrains;
  if (countDrains === 1) {
    console.error('drain=1, write again');
    assert.equal(fs.readFileSync(filepath, 'utf8'), EXPECTED);
    console.error('ondrain write ret=%j', file.write(EXPECTED));
    cb_occurred += 'write ';
  } else if (countDrains == 2) {
    console.error('second drain, end');
    assert.equal(fs.readFileSync(filepath, 'utf8'), EXPECTED + EXPECTED);
    file.end();
  }
});

file.on('close', function() {
  cb_occurred += 'close ';
  assert.strictEqual(file.bytesWritten, EXPECTED.length * 2);
  file.write('should not work anymore');
});


file.on('error', function(err) {
  cb_occurred += 'error ';
  assert.ok(err.message.indexOf('write after end') >= 0);
});


for (var i = 0; i < 11; i++) {
  var ret = file.write(i + '');
  console.error('%d %j', i, ret);

  // return false when i hits 10
  assert(ret === (i != 10));
}
cb_occurred += 'write ';
