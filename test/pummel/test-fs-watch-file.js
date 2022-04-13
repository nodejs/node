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

const tmpdir = require('../common/tmpdir');

let watchSeenOne = 0;
let watchSeenTwo = 0;
let watchSeenThree = 0;
let watchSeenFour = 0;

const testDir = tmpdir.path;

const filenameOne = 'watch.txt';
const filepathOne = path.join(testDir, filenameOne);

const filenameTwo = 'hasOwnProperty';
const filepathTwo = filenameTwo;
const filepathTwoAbs = path.join(testDir, filenameTwo);

const filenameThree = 'charm'; // Because the third time is

const filenameFour = 'get';

process.on('exit', function() {
  assert.strictEqual(watchSeenOne, 1);
  assert.strictEqual(watchSeenTwo, 2);
  assert.strictEqual(watchSeenThree, 1);
  assert.strictEqual(watchSeenFour, 1);
});


tmpdir.refresh();
fs.writeFileSync(filepathOne, 'hello');

assert.throws(
  () => { fs.watchFile(filepathOne); },
  { code: 'ERR_INVALID_ARG_TYPE' }
);

// Does not throw.
fs.watchFile(filepathOne, function() {
  fs.unwatchFile(filepathOne);
  ++watchSeenOne;
});

setTimeout(function() {
  fs.writeFileSync(filepathOne, 'world');
}, 1000);


process.chdir(testDir);

fs.writeFileSync(filepathTwoAbs, 'howdy');

assert.throws(
  () => { fs.watchFile(filepathTwo); },
  { code: 'ERR_INVALID_ARG_TYPE' }
);

{ // Does not throw.
  function a() {
    fs.unwatchFile(filepathTwo, a);
    ++watchSeenTwo;
  }

  function b() {
    fs.unwatchFile(filepathTwo, b);
    ++watchSeenTwo;
  }
  fs.watchFile(filepathTwo, a);
  fs.watchFile(filepathTwo, b);
}

setTimeout(function() {
  fs.writeFileSync(filepathTwoAbs, 'pardner');
}, 1000);

{ // Does not throw.
  function b() {
    fs.unwatchFile(filenameThree, b);
    ++watchSeenThree;
  }
  const uncalledListener = common.mustNotCall();
  fs.watchFile(filenameThree, uncalledListener);
  fs.watchFile(filenameThree, b);
  fs.unwatchFile(filenameThree, uncalledListener);
}

setTimeout(function() {
  fs.writeFileSync(filenameThree, 'pardner');
}, 1000);

setTimeout(function() {
  fs.writeFileSync(filenameFour, 'hey');
}, 200);

setTimeout(function() {
  fs.writeFileSync(filenameFour, 'hey');
}, 500);

{ // Does not throw.
  function a() {
    ++watchSeenFour;
    assert.strictEqual(watchSeenFour, 1);
    fs.unwatchFile(`.${path.sep}${filenameFour}`, a);
  }
  fs.watchFile(filenameFour, a);
}
