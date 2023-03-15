'use strict';

const finished = require('./end-of-stream');
const kLastResolve = Symbol('lastResolve');
const kLastReject = Symbol('lastReject');
const kError = Symbol('error');
const kEnded = Symbol('ended');
const kLastPromise = Symbol('lastPromise');
const kHandlePromise = Symbol('handlePromise');
const kStream = Symbol('stream');
function createIterResult(value, done) {
  return {
    value,
    done
  };
}
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
      resolve(createIterResult(data, false));
    }
  }
}
function onReadable(iter) {
  // we wait for the next tick, because it might
  // emit an error with process.nextTick
  process.nextTick(readAndResolve, iter);
}
function wrapForNext(lastPromise, iter) {
  return (resolve, reject) => {
    lastPromise.then(() => {
      if (iter[kEnded]) {
        resolve(createIterResult(undefined, true));
        return;
      }
      iter[kHandlePromise](resolve, reject);
    }, reject);
  };
}
const AsyncIteratorPrototype = Object.getPrototypeOf(function () {});
const ReadableStreamAsyncIteratorPrototype = Object.setPrototypeOf({
  get stream() {
    return this[kStream];
  },
  next() {
    // if we have detected an error in the meanwhile
    // reject straight away
    const error = this[kError];
    if (error !== null) {
      return Promise.reject(error);
    }
    if (this[kEnded]) {
      return Promise.resolve(createIterResult(undefined, true));
    }
    if (this[kStream].destroyed) {
      // We need to defer via nextTick because if .destroy(err) is
      // called, the error will be emitted via nextTick, and
      // we cannot guarantee that there is no error lingering around
      // waiting to be emitted.
      return new Promise((resolve, reject) => {
        process.nextTick(() => {
          if (this[kError]) {
            reject(this[kError]);
          } else {
            resolve(createIterResult(undefined, true));
          }
        });
      });
    }

    // if we have multiple next() calls
    // we will wait for the previous Promise to finish
    // this logic is optimized to support for await loops,
    // where next() is only called once at a time
    const lastPromise = this[kLastPromise];
    let promise;
    if (lastPromise) {
      promise = new Promise(wrapForNext(lastPromise, this));
    } else {
      // fast path needed to support multiple this.push()
      // without triggering the next() queue
      const data = this[kStream].read();
      if (data !== null) {
        return Promise.resolve(createIterResult(data, false));
      }
      promise = new Promise(this[kHandlePromise]);
    }
    this[kLastPromise] = promise;
    return promise;
  },
  [Symbol.asyncIterator]() {
    return this;
  },
  return() {
    // destroy(err, cb) is a private API
    // we can guarantee we have that here, because we control the
    // Readable class this is attached to
    return new Promise((resolve, reject) => {
      this[kStream].destroy(null, err => {
        if (err) {
          reject(err);
          return;
        }
        resolve(createIterResult(undefined, true));
      });
    });
  }
}, AsyncIteratorPrototype);
const createReadableStreamAsyncIterator = stream => {
  const iterator = Object.create(ReadableStreamAsyncIteratorPrototype, {
    [kStream]: {
      value: stream,
      writable: true
    },
    [kLastResolve]: {
      value: null,
      writable: true
    },
    [kLastReject]: {
      value: null,
      writable: true
    },
    [kError]: {
      value: null,
      writable: true
    },
    [kEnded]: {
      value: stream._readableState.endEmitted,
      writable: true
    },
    // the function passed to new Promise
    // is cached so we avoid allocating a new
    // closure at every run
    [kHandlePromise]: {
      value: (resolve, reject) => {
        const data = iterator[kStream].read();
        if (data) {
          iterator[kLastPromise] = null;
          iterator[kLastResolve] = null;
          iterator[kLastReject] = null;
          resolve(createIterResult(data, false));
        } else {
          iterator[kLastResolve] = resolve;
          iterator[kLastReject] = reject;
        }
      },
      writable: true
    }
  });
  iterator[kLastPromise] = null;
  finished(stream, err => {
    if (err && err.code !== 'ERR_STREAM_PREMATURE_CLOSE') {
      const reject = iterator[kLastReject];
      // reject if we are waiting for data in the Promise
      // returned by next() and store the error
      if (reject !== null) {
        iterator[kLastPromise] = null;
        iterator[kLastResolve] = null;
        iterator[kLastReject] = null;
        reject(err);
      }
      iterator[kError] = err;
      return;
    }
    const resolve = iterator[kLastResolve];
    if (resolve !== null) {
      iterator[kLastPromise] = null;
      iterator[kLastResolve] = null;
      iterator[kLastReject] = null;
      resolve(createIterResult(undefined, true));
    }
    iterator[kEnded] = true;
  });
  stream.on('readable', onReadable.bind(null, iterator));
  return iterator;
};
module.exports = createReadableStreamAsyncIterator;