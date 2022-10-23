'use strict';

require('../common');

const assert = require('assert');

assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(WritableStream.prototype, Symbol.toStringTag),
  { configurable: true, enumerable: false, value: 'WritableStream', writable: false },
);
assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(WritableStreamDefaultWriter.prototype, Symbol.toStringTag),
  { configurable: true, enumerable: false, value: 'WritableStreamDefaultWriter', writable: false },
);
assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(WritableStreamDefaultController.prototype, Symbol.toStringTag),
  { configurable: true, enumerable: false, value: 'WritableStreamDefaultController', writable: false },
);

assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(ReadableStream.prototype, Symbol.toStringTag),
  { configurable: true, enumerable: false, value: 'ReadableStream', writable: false },
);
assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(ReadableStreamBYOBRequest.prototype, Symbol.toStringTag),
  { configurable: true, enumerable: false, value: 'ReadableStreamBYOBRequest', writable: false },
);
assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(ReadableStreamDefaultReader.prototype, Symbol.toStringTag),
  { configurable: true, enumerable: false, value: 'ReadableStreamDefaultReader', writable: false },
);
assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(ReadableStreamBYOBReader.prototype, Symbol.toStringTag),
  { configurable: true, enumerable: false, value: 'ReadableStreamBYOBReader', writable: false },
);
assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(ReadableStreamDefaultController.prototype, Symbol.toStringTag),
  { configurable: true, enumerable: false, value: 'ReadableStreamDefaultController', writable: false },
);
assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(ReadableByteStreamController.prototype, Symbol.toStringTag),
  { configurable: true, enumerable: false, value: 'ReadableByteStreamController', writable: false },
);

assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(ByteLengthQueuingStrategy.prototype, Symbol.toStringTag),
  { configurable: true, enumerable: false, value: 'ByteLengthQueuingStrategy', writable: false },
);
assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(CountQueuingStrategy.prototype, Symbol.toStringTag),
  { configurable: true, enumerable: false, value: 'CountQueuingStrategy', writable: false },
);

assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(TransformStream.prototype, Symbol.toStringTag),
  { configurable: true, enumerable: false, value: 'TransformStream', writable: false },
);
assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(TransformStreamDefaultController.prototype, Symbol.toStringTag),
  { configurable: true, enumerable: false, value: 'TransformStreamDefaultController', writable: false },
);
