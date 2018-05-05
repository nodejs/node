'use strict';

const kLastResolve = Symbol('lastResolve');
const kLastReject = Symbol('lastReject');
const kError = Symbol('error');
const kEnded = Symbol('ended');
const kLastPromise = Symbol('lastPromise');
const kHandlePromise = Symbol('handlePromise');
const kStream = Symbol('stream');
const {
  ERR_METHOD_NOT_IMPLEMENTED
} = require('internal/errors').codes;

const AsyncIteratorRecord = class AsyncIteratorRecord {
  constructor(value, done) {
    this.done = done;
    this.value = value;
  }
};

function readAndResolve(iter) {
  const resolve = iter[kLastResolve];
  if (resolve !== null) {
    const data = iter[kStream].read();
    // we defer if data is null
    // we can be expecting either 'end' or
    // 'error'
    if (data !== null) {
      iter[kLastPromise] = null;
      iter[kLastResolve] = null;
      iter[kLastReject] = null;
      resolve(new AsyncIteratorRecord(data, false));
    }
  }
}

function onReadable(iter) {
  // we wait for the next tick, because it might
  // emit an error with process.nextTick
  process.nextTick(readAndResolve, iter);
}

function onEnd(iter) {
  const resolve = iter[kLastResolve];
  if (resolve !== null) {
    iter[kLastPromise] = null;
    iter[kLastResolve] = null;
    iter[kLastReject] = null;
    resolve(new AsyncIteratorRecord(null, true));
  }
  iter[kEnded] = true;
}

function onError(iter, err) {
  const reject = iter[kLastReject];
  // reject if we are waiting for data in the Promise
  // returned by next() and store the error
  if (reject !== null) {
    iter[kLastPromise] = null;
    iter[kLastResolve] = null;
    iter[kLastReject] = null;
    reject(err);
  }
  iter[kError] = err;
}

function wrapForNext(lastPromise, iter) {
  return function(resolve, reject) {
    lastPromise.then(function() {
      iter[kHandlePromise](resolve, reject);
    }, reject);
  };
}

// Abstract class. Methods should be overridden
// in specific implementation classes
const BaseAsyncIterator = class BaseAsyncIterator {
  constructor(stream) {}

  get stream() {
    throw new ERR_METHOD_NOT_IMPLEMENTED('stream()');
  }

  next() {
    throw new ERR_METHOD_NOT_IMPLEMENTED('next()');
  }

  return() {
    throw new ERR_METHOD_NOT_IMPLEMENTED('return()');
  }
};

module.exports = {
  kLastResolve,
  kLastReject,
  kError,
  kEnded,
  kLastPromise,
  kHandlePromise,
  kStream,
  onReadable,
  onEnd,
  onError,
  wrapForNext,
  BaseAsyncIterator,
  AsyncIteratorRecord
};
