'use strict';

require('../common');

const { TextEncoderStream, TextDecoderStream } = require('stream/web');
const util = require('util');
const assert = require('assert');

const textEncoderStream = new TextEncoderStream();
assert.strictEqual(
  util.inspect(textEncoderStream),
  `TextEncoderStream {
  encoding: 'utf-8',
  readable: ReadableStream { locked: false, state: 'readable', supportsBYOB: false },
  writable: WritableStream { locked: false, state: 'writable' }
}`
);
assert.throws(() => textEncoderStream[util.inspect.custom].call(), {
  code: 'ERR_INVALID_THIS',
});

const textDecoderStream = new TextDecoderStream();
assert.strictEqual(
  util.inspect(textDecoderStream),
  `TextDecoderStream {
  encoding: 'utf-8',
  fatal: false,
  ignoreBOM: false,
  readable: ReadableStream { locked: false, state: 'readable', supportsBYOB: false },
  writable: WritableStream { locked: false, state: 'writable' }
}`
);
assert.throws(() => textDecoderStream[util.inspect.custom].call(), {
  code: 'ERR_INVALID_THIS',
});
