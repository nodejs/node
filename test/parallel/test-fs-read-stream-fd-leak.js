'use strict';

var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var path = require('path');

var openCount = 0;
var _fsopen = fs.open;
var _fsclose = fs.close;

var loopCount = 50;
var totalCheck = 50;
var emptyTxt = path.join(common.fixturesDir, 'empty.txt');

fs.open = function() {
  openCount++;
  return _fsopen.apply(null, arguments);
};

fs.close = function() {
  openCount--;
  return _fsclose.apply(null, arguments);
};

function testLeak(endFn, callback) {
  console.log('testing for leaks from fs.createReadStream().%s()...', endFn);

  var i = 0;
  var check = 0;

  var checkFunction = function() {
    if (openCount != 0 && check < totalCheck) {
      check++;
      setTimeout(checkFunction, 100);
      return;
    }
    assert.equal(0, openCount, 'no leaked file descriptors using ' +
                 endFn + '() (got ' + openCount + ')');
    openCount = 0;
    callback && setTimeout(callback, 100);
  };

  setInterval(function() {
    var s = fs.createReadStream(emptyTxt);
    s[endFn]();

    if (++i === loopCount) {
      clearTimeout(this);
      setTimeout(checkFunction, 100);
    }
  }, 2);
}

testLeak('close', function() {
  testLeak('destroy');
});
