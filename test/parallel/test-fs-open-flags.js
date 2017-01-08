'use strict';
require('../common');
const assert = require('assert');

const fs = require('fs');

const O_APPEND = fs.constants.O_APPEND || 0;
const O_CREAT = fs.constants.O_CREAT || 0;
const O_EXCL = fs.constants.O_EXCL || 0;
const O_RDONLY = fs.constants.O_RDONLY || 0;
const O_RDWR = fs.constants.O_RDWR || 0;
const O_TRUNC = fs.constants.O_TRUNC || 0;
const O_WRONLY = fs.constants.O_WRONLY || 0;

assert.strictEqual(fs._stringToFlags('r'), O_RDONLY);
assert.strictEqual(fs._stringToFlags('r+'), O_RDWR);
assert.strictEqual(fs._stringToFlags('w'), O_TRUNC | O_CREAT | O_WRONLY);
assert.strictEqual(fs._stringToFlags('w+'), O_TRUNC | O_CREAT | O_RDWR);
assert.strictEqual(fs._stringToFlags('a'), O_APPEND | O_CREAT | O_WRONLY);
assert.strictEqual(fs._stringToFlags('a+'), O_APPEND | O_CREAT | O_RDWR);

assert.strictEqual(fs._stringToFlags('wx'),
                   O_TRUNC | O_CREAT | O_WRONLY | O_EXCL);
assert.strictEqual(fs._stringToFlags('xw'),
                   O_TRUNC | O_CREAT | O_WRONLY | O_EXCL);
assert.strictEqual(fs._stringToFlags('wx+'),
                   O_TRUNC | O_CREAT | O_RDWR | O_EXCL);
assert.strictEqual(fs._stringToFlags('xw+'),
                   O_TRUNC | O_CREAT | O_RDWR | O_EXCL);
assert.strictEqual(fs._stringToFlags('ax'),
                   O_APPEND | O_CREAT | O_WRONLY | O_EXCL);
assert.strictEqual(fs._stringToFlags('xa'),
                   O_APPEND | O_CREAT | O_WRONLY | O_EXCL);
assert.strictEqual(fs._stringToFlags('ax+'),
                   O_APPEND | O_CREAT | O_RDWR | O_EXCL);
assert.strictEqual(fs._stringToFlags('xa+'),
                   O_APPEND | O_CREAT | O_RDWR | O_EXCL);

('+ +a +r +w rw wa war raw r++ a++ w++ x +x x+ rx rx+ wxx wax xwx xxx')
  .split(' ')
  .forEach(function(flags) {
    assert.throws(function() { fs._stringToFlags(flags); });
  });

assert.throws(
  () => fs._stringToFlags({}),
  /Unknown file open flag: \[object Object\]/
);

assert.throws(
  () => fs._stringToFlags(true),
  /Unknown file open flag: true/
);

assert.throws(
  () => fs._stringToFlags(null),
  /Unknown file open flag: null/
);
