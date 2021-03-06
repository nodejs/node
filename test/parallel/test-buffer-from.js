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
  {},
  new Boolean(true),
  { valueOf() { return null; } },
  { valueOf() { return undefined; } },
  { valueOf: null },
  Object.create(null),
  new Number(true),
  new MyBadPrimitive(),
  Symbol(),
  5n,
  (one, two, three) => {},
  undefined,
  null
].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The first argument must be of type string or an instance of ' +
             'Buffer, ArrayBuffer, or Array or an Array-like Object.' +
             common.invalidArgTypeHelper(input)
  };
  throws(() => Buffer.from(input), errObj);
  throws(() => Buffer.from(input, 'hex'), errObj);
});

Buffer.allocUnsafe(10); // Should not throw.
Buffer.from('deadbeaf', 'hex'); // Should not throw.
