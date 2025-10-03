'use strict';
require('../common');

const { describe, it } = require('node:test');
const { deepStrictEqual, throws } = require('node:assert');

const { diff } = require('util');

describe('diff', () => {
  it('throws because actual is nor an array nor a string', () => {
    const actual = {};
    const expected = 'foo';

    throws(() => diff(actual, expected), {
      message: 'The "actual" argument must be of type string. Received an instance of Object'
    });
  });

  it('throws because expected is nor an array nor a string', () => {
    const actual = 'foo';
    const expected = {};

    throws(() => diff(actual, expected), {
      message: 'The "expected" argument must be of type string. Received an instance of Object'
    });
  });


  it('throws because the actual array does not only contain string', () => {
    const actual = ['1', { b: 2 }];
    const expected = ['1', '2'];

    throws(() => diff(actual, expected), {
      message: 'The "actual[1]" argument must be of type string. Received an instance of Object'
    });
  });

  it('returns an empty array because actual and expected are the same', () => {
    const actual = 'foo';
    const expected = 'foo';

    const result = diff(actual, expected);
    deepStrictEqual(result, []);
  });

  it('returns the diff for strings', () => {
    const actual = '12345678';
    const expected = '12!!5!7!';
    const result = diff(actual, expected);

    deepStrictEqual(result, [
      [0, '1'],
      [0, '2'],
      [1, '3'],
      [1, '4'],
      [-1, '!'],
      [-1, '!'],
      [0, '5'],
      [1, '6'],
      [-1, '!'],
      [0, '7'],
      [1, '8'],
      [-1, '!'],
    ]);
  });

  it('returns the diff for arrays', () => {
    const actual = ['1', '2', '3'];
    const expected = ['1', '3', '4'];
    const result = diff(actual, expected);

    deepStrictEqual(result, [
      [0, '1'],
      [1, '2'],
      [0, '3'],
      [-1, '4'],
    ]
    );
  });
});
