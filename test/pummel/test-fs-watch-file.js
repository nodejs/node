'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

var watchSeenOne = 0;
var watchSeenTwo = 0;
var watchSeenThree = 0;
var watchSeenFour = 0;

var testDir = common.tmpDir;

var filenameOne = 'watch.txt';
var filepathOne = path.join(testDir, filenameOne);

var filenameTwo = 'hasOwnProperty';
var filepathTwo = filenameTwo;
var filepathTwoAbs = path.join(testDir, filenameTwo);

var filenameThree = 'charm'; // because the third time is

var filenameFour = 'get';

process.on('exit', function() {
  fs.unlinkSync(filepathOne);
  fs.unlinkSync(filepathTwoAbs);
  fs.unlinkSync(filenameThree);
  fs.unlinkSync(filenameFour);
  assert.equal(1, watchSeenOne);
  assert.equal(2, watchSeenTwo);
  assert.equal(1, watchSeenThree);
  assert.equal(1, watchSeenFour);
});


fs.writeFileSync(filepathOne, 'hello');

assert.throws(
    function() {
      fs.watchFile(filepathOne);
    },
    function(e) {
      return e.message === 'watchFile requires a listener function';
    }
);

assert.doesNotThrow(
    function() {
      fs.watchFile(filepathOne, function(curr, prev) {
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
      return e.message === 'watchFile requires a listener function';
    }
);

assert.doesNotThrow(
    function() {
      function a(curr, prev) {
        fs.unwatchFile(filepathTwo, a);
        ++watchSeenTwo;
      }
      function b(curr, prev) {
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
      function a(curr, prev) {
        assert.ok(0); // should not run
      }
      function b(curr, prev) {
        fs.unwatchFile(filenameThree, b);
        ++watchSeenThree;
      }
      fs.watchFile(filenameThree, a);
      fs.watchFile(filenameThree, b);
      fs.unwatchFile(filenameThree, a);
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
      function a(curr, prev) {
        ++watchSeenFour;
        assert.equal(1, watchSeenFour);
        fs.unwatchFile('.' + path.sep + filenameFour, a);
      }
      fs.watchFile(filenameFour, a);
    }
);
