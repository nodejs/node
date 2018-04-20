'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');

// This test ensures that appropriate TypeError is thrown by createReadStream
// when an argument with invalid type is passed

const example = fixtures.path('x.txt');
// Should not throw.
fs.createReadStream(example, undefined);
fs.createReadStream(example, null);
fs.createReadStream(example, 'utf8');
fs.createReadStream(example, { encoding: 'utf8' });

const createReadStreamErr = (path, opt, error) => {
  common.expectsError(() => {
    fs.createReadStream(path, opt);
  }, error);
};

const typeError = {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError
};

const rangeError = {
  code: 'ERR_OUT_OF_RANGE',
  type: RangeError
};

createReadStreamErr(example, 123, typeError);
createReadStreamErr(example, 0, typeError);
createReadStreamErr(example, true, typeError);
createReadStreamErr(example, false, typeError);

// Case 0: Should not throw if either start or end is undefined
fs.createReadStream(example, {});
fs.createReadStream(example, { start: 0 });
fs.createReadStream(example, { end: Infinity });

// Case 1: Should throw TypeError if either start or end is not of type 'number'
createReadStreamErr(example, { start: 'invalid' }, typeError);
createReadStreamErr(example, { end: 'invalid' }, typeError);
createReadStreamErr(example, { start: 'invalid', end: 'invalid' }, typeError);

// Case 2: Should throw RangeError if either start or end is NaN
createReadStreamErr(example, { start: NaN }, rangeError);
createReadStreamErr(example, { end: NaN }, rangeError);
createReadStreamErr(example, { start: NaN, end: NaN }, rangeError);

// Case 3: Should throw RangeError if either start or end is negative
createReadStreamErr(example, { start: -1 }, rangeError);
createReadStreamErr(example, { end: -1 }, rangeError);
createReadStreamErr(example, { start: -1, end: -1 }, rangeError);

// Case 4: Should throw RangeError if either start or end is fractional
createReadStreamErr(example, { start: 0.1 }, rangeError);
createReadStreamErr(example, { end: 0.1 }, rangeError);
createReadStreamErr(example, { start: 0.1, end: 0.1 }, rangeError);

// Case 5: Should not throw if both start and end are whole numbers
fs.createReadStream(example, { start: 1, end: 5 });

// Case 6: Should throw RangeError if start is greater than end
createReadStreamErr(example, { start: 5, end: 1 }, rangeError);
