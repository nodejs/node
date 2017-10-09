'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');

const example = fixtures.path('x.txt');

assert.doesNotThrow(function() {
  fs.createReadStream(example, undefined);
});
assert.doesNotThrow(function() {
  fs.createReadStream(example, null);
});
assert.doesNotThrow(function() {
  fs.createReadStream(example, 'utf8');
});
assert.doesNotThrow(function() {
  fs.createReadStream(example, { encoding: 'utf8' });
});

const createReadStreamErr = (path, opt) => {
  common.expectsError(
    () => {
      fs.createReadStream(path, opt);
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    });
};

createReadStreamErr(example, 123);
createReadStreamErr(example, 0);
createReadStreamErr(example, true);
createReadStreamErr(example, false);
