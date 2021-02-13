'use strict';

const {
  ArrayPrototypePop,
  Promise,
  SymbolAsyncIterator,
  SymbolIterator,
} = primordials;

const {
  addAbortSignalNoValidate,
} = require('internal/streams/add-abort-signal');

const {
  validateAbortSignal,
} = require('internal/validators');

let pl;
let eos;

function isReadable(obj) {
  return !!(obj && typeof obj.pipe === 'function');
}

function isWritable(obj) {
  return !!(obj && typeof obj.write === 'function');
}

function isStream(obj) {
  return isReadable(obj) || isWritable(obj);
}

function isIterable(obj, isAsync) {
  if (!obj) return false;
  if (isAsync === true) return typeof obj[SymbolAsyncIterator] === 'function';
  if (isAsync === false) return typeof obj[SymbolIterator] === 'function';
  return typeof obj[SymbolAsyncIterator] === 'function' ||
    typeof obj[SymbolIterator] === 'function';
}

function pipeline(...streams) {
  if (!pl) pl = require('internal/streams/pipeline');
  return new Promise((resolve, reject) => {
    let signal;
    const lastArg = streams[streams.length - 1];
    if (lastArg && typeof lastArg === 'object' &&
        !isStream(lastArg) && !isIterable(lastArg)) {
      const options = ArrayPrototypePop(streams);
      signal = options.signal;
      validateAbortSignal(signal, 'options.signal');
    }

    const pipe = pl(...streams, (err, value) => {
      if (err) {
        reject(err);
      } else {
        resolve(value);
      }
    });
    if (signal) {
      addAbortSignalNoValidate(signal, pipe);
    }
  });
}

function finished(stream, opts) {
  if (!eos) eos = require('internal/streams/end-of-stream');
  return new Promise((resolve, reject) => {
    eos(stream, opts, (err) => {
      if (err) {
        reject(err);
      } else {
        resolve();
      }
    });
  });
}

module.exports = {
  finished,
  pipeline,
};
