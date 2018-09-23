'use strict';

const common = require('../common');
const { deepStrictEqual, throws } = require('assert');
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
deepStrictEqual(
  Buffer.from(runInNewContext('new String(checkString)', { checkString })),
  check
);

[
  [{}, 'object'],
  [new Boolean(true), 'boolean'],
  [{ valueOf() { return null; } }, 'object'],
  [{ valueOf() { return undefined; } }, 'object'],
  [{ valueOf: null }, 'object'],
  [Object.create(null), 'object']
].forEach(([input, actualType]) => {
  const err = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The first argument must be one of type string, Buffer, ' +
             'ArrayBuffer, Array, or Array-like Object. Received ' +
             `type ${actualType}`
  });
  throws(() => Buffer.from(input), err);
});

[
  new Number(true),
  new MyBadPrimitive()
].forEach((input) => {
  const errMsg = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "value" argument must not be of type number. ' +
             'Received type number'
  });
  throws(() => Buffer.from(input), errMsg);
});
