'use strict';

const {
  TransformStream,
  TransformStreamDefaultController,
} = require('internal/webstreams/transformstream');

const {
  WritableStream,
  WritableStreamDefaultController,
  WritableStreamDefaultWriter,
} = require('internal/webstreams/writablestream');

const {
  ReadableStream,
  ReadableStreamDefaultReader,
  ReadableStreamBYOBReader,
  ReadableStreamBYOBRequest,
  ReadableByteStreamController,
  ReadableStreamDefaultController,
  isReadableStream,
  readableStreamTee,
} = require('internal/webstreams/readablestream');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');

const {
  validateBoolean,
} = require('internal/validators');

const {
  ByteLengthQueuingStrategy,
  CountQueuingStrategy,
} = require('internal/webstreams/queuingstrategies');

const {
  TextEncoderStream,
  TextDecoderStream,
} = require('internal/webstreams/encoding');

const {
  CompressionStream,
  DecompressionStream,
} = require('internal/webstreams/compression');

function ReadableStreamTee(stream, cloneForBranch2 = false) {
  if (!isReadableStream(stream)) {
    throw new ERR_INVALID_ARG_TYPE('stream', 'ReadableStream', stream);
  }
  validateBoolean(cloneForBranch2, 'cloneForBranch2');
  return readableStreamTee(stream, cloneForBranch2);
}

module.exports = {
  ReadableStream,
  ReadableStreamTee,
  ReadableStreamDefaultReader,
  ReadableStreamBYOBReader,
  ReadableStreamBYOBRequest,
  ReadableByteStreamController,
  ReadableStreamDefaultController,
  TransformStream,
  TransformStreamDefaultController,
  WritableStream,
  WritableStreamDefaultWriter,
  WritableStreamDefaultController,
  ByteLengthQueuingStrategy,
  CountQueuingStrategy,
  TextEncoderStream,
  TextDecoderStream,
  CompressionStream,
  DecompressionStream,
};
