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

let watchSeenOne = 0;
let watchSeenTwo = 0;
let watchSeenThree = 0;
let watchSeenFour = 0;

const testDir = common.tmpDir;

const filenameOne = 'watch.txt';
const filepathOne = path.join(testDir, filenameOne);

const filenameTwo = 'hasOwnProperty';
const filepathTwo = filenameTwo;
const filepathTwoAbs = path.join(testDir, filenameTwo);

const filenameThree = 'charm'; // because the third time is

const filenameFour = 'get';

process.on('exit', function() {
  fs.unlinkSync(filepathOne);
  fs.unlinkSync(filepathTwoAbs);
  fs.unlinkSync(filenameThree);
  fs.unlinkSync(filenameFour);
  assert.strictEqual(1, watchSeenOne);
  assert.strictEqual(2, watchSeenTwo);
  assert.strictEqual(1, watchSeenThree);
  assert.strictEqual(1, watchSeenFour);
});


fs.writeFileSync(filepathOne, 'hello');

assert.throws(
  function() {
    fs.watchFile(filepathOne);
  },
  function(e) {
    return e.message === '"watchFile()" requires a listener function';
  }
);

assert.doesNotThrow(
  function() {
    fs.watchFile(filepathOne, function() {
      fs.unwatchFile(filepathOne);
      ++watchSeenOne;
    });
  }
);

setTimeout(function() {
  fs.writeFileSync(filepathOne, 'world');
}, 1000);


process.chdir(testDir);

fs.writeFileSync(filepathTwoAbs, 'howdy');

assert.throws(
  function() {
    fs.watchFile(filepathTwo);
  },
  function(e) {
    return e.message === '"watchFile()" requires a listener function';
  }
);

assert.doesNotThrow(
  function() {
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
);

setTimeout(function() {
  fs.writeFileSync(filepathTwoAbs, 'pardner');
}, 1000);

assert.doesNotThrow(
  function() {
    function b() {
      fs.unwatchFile(filenameThree, b);
      ++watchSeenThree;
    }
    fs.watchFile(filenameThree, common.mustNotCall());
    fs.watchFile(filenameThree, b);
    fs.unwatchFile(filenameThree, common.mustNotCall());
  }
);

setTimeout(function() {
  fs.writeFileSync(filenameThree, 'pardner');
}, 1000);

setTimeout(function() {
  fs.writeFileSync(filenameFour, 'hey');
}, 200);

setTimeout(function() {
  fs.writeFileSync(filenameFour, 'hey');
}, 500);

assert.doesNotThrow(
  function() {
    function a() {
      ++watchSeenFour;
      assert.strictEqual(1, watchSeenFour);
      fs.unwatchFile(`.${path.sep}${filenameFour}`, a);
    }
    fs.watchFile(filenameFour, a);
  }
);
