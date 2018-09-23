'use strict';

const kLastResolve = Symbol('lastResolve');
const kLastReject = Symbol('lastReject');
const kError = Symbol('error');
const kEnded = Symbol('ended');
const kLastPromise = Symbol('lastPromise');
const kHandlePromise = Symbol('handlePromise');
const kStream = Symbol('stream');

function createIterResult(value, done) {
  return { value, done };
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

function onEnd(iter) {
  const resolve = iter[kLastResolve];
  if (resolve !== null) {
    iter[kLastPromise] = null;
    iter[kLastResolve] = null;
    iter[kLastReject] = null;
    resolve(createIterResult(null, true));
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

const AsyncIteratorPrototype = Object.getPrototypeOf(
  Object.getPrototypeOf(async function* () {}).prototype);

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
      return Promise.resolve(createIterResult(null, true));
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

  return() {
    // destroy(err, cb) is a private API
    // we can guarantee we have that here, because we control the
    // Readable class this is attached to
    return new Promise((resolve, reject) => {
      this[kStream].destroy(null, (err) => {
        if (err) {
          reject(err);
          return;
        }
        resolve(createIterResult(null, true));
      });
    });
  },
}, AsyncIteratorPrototype);

const createReadableStreamAsyncIterator = (stream) => {
  const iterator = Object.create(ReadableStreamAsyncIteratorPrototype, {
    [kStream]: { value: stream, writable: true },
    [kLastResolve]: { value: null, writable: true },
    [kLastReject]: { value: null, writable: true },
    [kError]: { value: null, writable: true },
    [kEnded]: { value: false, writable: true },
    [kLastPromise]: { value: null, writable: true },
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
      writable: true,
    },
  });

  stream.on('readable', onReadable.bind(null, iterator));
  stream.on('end', onEnd.bind(null, iterator));
  stream.on('error', onError.bind(null, iterator));

  return iterator;
};

module.exports = createReadableStreamAsyncIterator;
