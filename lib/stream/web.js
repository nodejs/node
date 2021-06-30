'use strict';

const {
  emitExperimentalWarning,
} = require('internal/util');

emitExperimentalWarning('stream/web');

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
} = require('internal/webstreams/readablestream');

const {
  ByteLengthQueuingStrategy,
  CountQueuingStrategy,
} = require('internal/webstreams/queuingstrategies');

module.exports = {
  ReadableStream,
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
};
