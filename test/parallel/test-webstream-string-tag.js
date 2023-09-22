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

/* Commenting forEach Loop as per the document */
// classesToBeTested.forEach((cls) => {
//   assert.strictEqual(cls.prototype[Symbol.toStringTag], cls.name);
//   assert.deepStrictEqual(Object.getOwnPropertyDescriptor(cls.prototype, Symbol.toStringTag),
//                          { configurable: true, enumerable: false, value: cls.name, writable: false });
// });

/* Adding for loop with all the test included in the above code */
for (let i = 0; i < classesToBeTested.length; i++) {
  const cls = classesToBeTested[i];
  assert.strictEqual(cls.prototype[Symbol.toStringTag], cls.name);
  assert.deepStrictEqual(
    Object.getOwnPropertyDescriptor(cls.prototype, Symbol.toStringTag),
    {
      configurable: true,
      enumerable: false,
      value: cls.name,
      writable: false,
    }
  );
}
