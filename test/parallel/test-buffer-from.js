'use strict';

require('../common');
const { deepStrictEqual, throws } = require('assert');
const { Buffer } = require('buffer');
const { runInNewContext } = require('vm');

const checkString = 'test';

const check = Buffer.from(checkString);

class MyString extends String {
  constructor() {
    super(checkString);
  }
}

class MyPrimitive {
  [Symbol.toPrimitive]() {
    return checkString;
  }
}

class MyBadPrimitive {
  [Symbol.toPrimitive]() {
    return 1;
  }
}

deepStrictEqual(Buffer.from(new String(checkString)), check);
deepStrictEqual(Buffer.from(new MyString()), check);
deepStrictEqual(Buffer.from(new MyPrimitive()), check);
deepStrictEqual(Buffer.from(
  runInNewContext('new String(checkString)', {checkString})),
                check);

const err = new RegExp('^TypeError: First argument must be a string, Buffer, ' +
                       'ArrayBuffer, Array, or array-like object\\.$');

[
  {},
  new Boolean(true),
  { valueOf() { return null; } },
  { valueOf() { return undefined; } },
  { valueOf: null },
  Object.create(null)
].forEach((input) => {
  throws(() => Buffer.from(input), err);
});

[
  new Number(true),
  new MyBadPrimitive()
].forEach((input) => {
  throws(() => Buffer.from(input),
         /^TypeError: "value" argument must not be a number$/);
});
