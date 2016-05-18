'use strict';
var common = require('../common');
var assert = require('assert');

var path = require('path');
var fs = require('fs');


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


common.refreshTmpDir();

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
