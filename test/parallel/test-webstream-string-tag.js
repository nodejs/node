'use strict';

require('../common');

const assert = require('assert');

const classesToBeTested = [
  WritableStream,
  WritableStreamDefaultWriter,
  WritableStreamDefaultController,
  ReadableStream,
  ReadableStreamBYOBRequest,
  ReadableStreamDefaultReader,
  ReadableStreamBYOBReader,
  ReadableStreamDefaultController,
  ReadableByteStreamController,
  ByteLengthQueuingStrategy,
  CountQueuingStrategy,
  TransformStream,
  TransformStreamDefaultController,
];

/* Replaced for loop with all the test included in the above code */
for (const cls of classesToBeTested) {
  const clsName = cls.name;

  assert.strictEqual(cls.prototype[Symbol.toStringTag], clsName);

  assert.deepStrictEqual(Object.getOwnPropertyDescriptor(cls.prototype, Symbol.toStringTag), {
    configurable: true,
    enumerable: false,
    value: clsName,
    writable: false
  });
}
