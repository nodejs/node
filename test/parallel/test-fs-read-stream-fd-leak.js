'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const fixtures = require('../common/fixtures');

let openCount = 0;
const _fsopen = fs.open;
const _fsclose = fs.close;

const loopCount = 50;
const totalCheck = 50;
const emptyTxt = fixtures.path('empty.txt');

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

  let i = 0;
  let check = 0;

  function checkFunction() {
    if (openCount !== 0 && check < totalCheck) {
      check++;
      setTimeout(checkFunction, 100);
      return;
    }

    assert.strictEqual(
      0,
      openCount,
      `no leaked file descriptors using ${endFn}() (got ${openCount})`
    );

    openCount = 0;
    callback && setTimeout(callback, 100);
  }

  setInterval(function() {
    const s = fs.createReadStream(emptyTxt);
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
