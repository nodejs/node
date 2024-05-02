'use strict';

require('../common');

const assert = require('assert');

const classesToBeTested = [ WritableStream, WritableStreamDefaultWriter, WritableStreamDefaultController,
                            ReadableStream, ReadableStreamBYOBRequest, ReadableStreamDefaultReader,
                            ReadableStreamBYOBReader, ReadableStreamDefaultController, ReadableByteStreamController,
                            ByteLengthQueuingStrategy, CountQueuingStrategy, TransformStream,
                            TransformStreamDefaultController];


classesToBeTested.forEach((cls) => {
  assert.strictEqual(cls.prototype[Symbol.toStringTag], cls.name);
  assert.deepStrictEqual(Object.getOwnPropertyDescriptor(cls.prototype, Symbol.toStringTag),
                         { configurable: true, enumerable: false, value: cls.name, writable: false });
});
