'use strict';
// This test was originally written to test a regression
// that was introduced by
// https://github.com/nodejs/node/pull/2288#issuecomment-179543894
require('../common');

const assert = require('assert');
const parse = require('querystring').parse;

/*
taken from express-js/body-parser
https://github.com/expressjs/body-parser/
blob/ed25264fb494cf0c8bc992b8257092cd4f694d5e/test/urlencoded.js#L636-L651
*/
function createManyParams(count) {
  let str = '';

  if (count === 0) {
    return str;
  }

  str += '0=0';

  for (let i = 1; i < count; i++) {
    const n = i.toString(36);
    str += `&${n}=${n}`;
  }

  return str;
}

const count = 10000;
const originalMaxLength = 1000;
const params = createManyParams(count);

// thealphanerd
// 27def4f introduced a change to parse that would cause Inifity
// to be passed to String.prototype.split as an argument for limit
// In this instance split will always return an empty array
// this test confirms that the output of parse is the expected length
// when passed Infinity as the argument for maxKeys
const resultInfinity = parse(params, undefined, undefined, {
  maxKeys: Infinity
});
const resultNaN = parse(params, undefined, undefined, {
  maxKeys: NaN
});
const resultInfinityString = parse(params, undefined, undefined, {
  maxKeys: 'Infinity'
});
const resultNaNString = parse(params, undefined, undefined, {
  maxKeys: 'NaN'
});

// Non Finite maxKeys should return the length of input
assert.strictEqual(Object.keys(resultInfinity).length, count);
assert.strictEqual(Object.keys(resultNaN).length, count);
// Strings maxKeys should return the maxLength
// defined by parses internals
assert.strictEqual(Object.keys(resultInfinityString).length, originalMaxLength);
assert.strictEqual(Object.keys(resultNaNString).length, originalMaxLength);
