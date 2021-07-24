'use strict';

const { ArrayIsArray } = primordials;

const { validateCallback } = require('internal/validators');
const { isTransformStream } = require('internal/webstreams/transformstream');
const { isReadableStream } = require('internal/webstreams/readablestream');
const { isWritableStream } = require('internal/webstreams/writablestream');
const { once } = require('internal/util');
const { codes } = require('internal/errors');
const { ERR_INVALID_ARG_TYPE, ERR_MISSING_ARGS } = codes;

function pipeline(...input) {
  const [streams, callback] = parseAndValidateInput(input);
  pipeStreams(streams).then(callback);
}

function parseAndValidateInput(input) {
  validateInput(input);
  const [streams, callback] = parseInput(input);

  validateStreams(streams);
  validateCallback(callback);
  return [streams, once(callback)];
}

function validateInput(input) {
  if (!input || input.length < 2) {
    throw new ERR_MISSING_ARGS('input');
  }
}

function parseInput(input) {
  const callback = popCallbackFromInput(input);
  const streams = unnestArrayIfIsArray(input);
  return [streams, callback];
}

function popCallbackFromInput(input) {
  return input.pop();
}

function unnestArrayIfIsArray(array) {
  if (ArrayIsArray(array[0]) && array.length === 1) {
    return array[0];
  }
  return array;
}

function validateStreams(streams) {
  throwErrorIfInsufficientStreams(streams);

  const allButFirstStream = streams.slice(1);
  throwErrorIfStreamsNotWritable(allButFirstStream);

  const allButLastStream = streams.slice(0, -1);
  throwErrorIfStreamsNotReadable(allButLastStream);
}

function throwErrorIfInsufficientStreams(streams) {
  if (streams.length < 2) {
    throw new ERR_MISSING_ARGS('streams');
  }
}

function throwErrorIfStreamsNotWritable(streams) {
  if (streams.some(cannotWriteToStream)) {
    throw new ERR_INVALID_ARG_TYPE(
      'streams',
      'WritableStream|TransformStream',
      streams
    );
  }
}

function cannotWriteToStream(stream) {
  return !(isWritableStream(stream) || isTransformStream(stream));
}

function throwErrorIfStreamsNotReadable(streams) {
  if (streams.some(cannotReadFromStream)) {
    throw new ERR_INVALID_ARG_TYPE(
      'streams',
      'ReadableStream|TransformStream',
      streams
    );
  }
}

function cannotReadFromStream(stream) {
  return !(isReadableStream(stream) || isTransformStream(stream));
}

async function pipeStreams(streams) {
  for (let i = 0; i < streams.length - 1; i++) {
    const source = streams[i];
    const dest = streams[i + 1];
    await pipeStreamPair(source, dest);
  }
}

async function pipeStreamPair(source, dest) {
  if (isTransformStream(source)) {
    return pipeStreamPairWithTransformSource(source, dest);
  }

  if (isTransformStream(dest)) {
    return source.pipeThrough(dest);
  }

  return source.pipeTo(dest);
}

async function pipeStreamPairWithTransformSource(source, dest) {
  throwErrorIfTransformStreamHasNoReadable(source);
  return pipeStreamPair(source.readable, dest);
}

function throwErrorIfTransformStreamHasNoReadable(transform) {
  const readable = transform?.readable;
  if (!isReadableStream(readable)) {
    throw new ERR_INVALID_ARG_TYPE(
      'transform.readable',
      'ReadableStream',
      readable
    );
  }
}

module.exports = {
  pipeline,
};
