'use strict';

require('../common');

const assert = require('assert');
const webstreams = require('stream/web');

assert.strictEqual(ReadableStream, webstreams.ReadableStream);
assert.strictEqual(ReadableStreamDefaultReader, webstreams.ReadableStreamDefaultReader);
assert.strictEqual(ReadableStreamBYOBReader, webstreams.ReadableStreamBYOBReader);
assert.strictEqual(ReadableStreamBYOBRequest, webstreams.ReadableStreamBYOBRequest);
assert.strictEqual(ReadableByteStreamController, webstreams.ReadableByteStreamController);
assert.strictEqual(ReadableStreamDefaultController, webstreams.ReadableStreamDefaultController);
assert.strictEqual(TransformStream, webstreams.TransformStream);
assert.strictEqual(TransformStreamDefaultController, webstreams.TransformStreamDefaultController);
assert.strictEqual(WritableStream, webstreams.WritableStream);
assert.strictEqual(WritableStreamDefaultWriter, webstreams.WritableStreamDefaultWriter);
assert.strictEqual(WritableStreamDefaultController, webstreams.WritableStreamDefaultController);
assert.strictEqual(ByteLengthQueuingStrategy, webstreams.ByteLengthQueuingStrategy);
assert.strictEqual(CountQueuingStrategy, webstreams.CountQueuingStrategy);
assert.strictEqual(TextEncoderStream, webstreams.TextEncoderStream);
assert.strictEqual(TextDecoderStream, webstreams.TextDecoderStream);
assert.strictEqual(CompressionStream, webstreams.CompressionStream);
assert.strictEqual(DecompressionStream, webstreams.DecompressionStream);
