'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('streams');

// Set Node.js flags required for the tests.
runner.setFlags(['--expose-internals']);

// Set a script that will be executed in the worker before running the tests.
runner.setInitScript(`
  const {
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
  } = require('stream/web');

  const { internalBinding } = require('internal/test/binding');
  const { DOMException } = internalBinding('messaging');
  global.DOMException = DOMException;
  global.ReadableStream = ReadableStream;
  global.ReadableStreamDefaultReader = ReadableStreamDefaultReader;
  global.ReadableStreamBYOBReader = ReadableStreamBYOBReader;
  global.ReadableStreamBYOBRequest = ReadableStreamBYOBRequest;
  global.ReadableByteStreamController = ReadableByteStreamController;
  global.ReadableStreamDefaultController = ReadableStreamDefaultController;
  global.TransformStream = TransformStream;
  global.TransformStreamDefaultController = TransformStreamDefaultController;
  global.WritableStream = WritableStream;
  global.WritableStreamDefaultWriter = WritableStreamDefaultWriter;
  global.WritableStreamDefaultController = WritableStreamDefaultController;
  global.ByteLengthQueuingStrategy = ByteLengthQueuingStrategy;
  global.CountQueuingStrategy = CountQueuingStrategy;
`);

runner.runJsTests();
