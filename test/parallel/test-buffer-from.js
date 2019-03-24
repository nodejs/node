'use strict';

require('../common');
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
  [Object.create(null), 'object'],
  [new Number(true), 'number'],
  [new MyBadPrimitive(), 'number'],
  [Symbol(), 'symbol'],
  [5n, 'bigint'],
  [(one, two, three) => {}, 'function'],
  [undefined, 'undefined'],
  [null, 'object']
].forEach(([input, actualType]) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The first argument must be one of type string, Buffer, ' +
             'ArrayBuffer, Array, or Array-like Object. Received ' +
             `type ${actualType}`
  };
  throws(() => Buffer.from(input), errObj);
  throws(() => Buffer.from(input, 'hex'), errObj);
});

Buffer.allocUnsafe(10); // Should not throw.
Buffer.from('deadbeaf', 'hex'); // Should not throw.
