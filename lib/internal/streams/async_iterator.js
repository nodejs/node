'use strict';

const kLastResolve = Symbol('lastResolve');
const kLastReject = Symbol('lastReject');
const kError = Symbol('error');
const kEnded = Symbol('ended');
const kLastPromise = Symbol('lastPromise');
const kHandlePromise = Symbol('handlePromise');
const kStream = Symbol('stream');

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

const ReadableAsyncIterator = class ReadableAsyncIterator {
  constructor(stream) {
    this[kStream] = stream;
    this[kLastResolve] = null;
    this[kLastReject] = null;
    this[kError] = null;
    this[kEnded] = false;
    this[kLastPromise] = null;

    stream.on('readable', onReadable.bind(null, this));
    stream.on('end', onEnd.bind(null, this));
    stream.on('error', onError.bind(null, this));

    // the function passed to new Promise
    // is cached so we avoid allocating a new
    // closure at every run
    this[kHandlePromise] = (resolve, reject) => {
      const data = this[kStream].read();
      if (data) {
        this[kLastPromise] = null;
        this[kLastResolve] = null;
        this[kLastReject] = null;
        resolve(new AsyncIteratorRecord(data, false));
      } else {
        this[kLastResolve] = resolve;
        this[kLastReject] = reject;
      }
    };
  }

  get stream() {
    return this[kStream];
  }

  next() {
    // if we have detected an error in the meanwhile
    // reject straight away
    const error = this[kError];
    if (error !== null) {
      return Promise.reject(error);
    }

    if (this[kEnded]) {
      return Promise.resolve(new AsyncIteratorRecord(null, true));
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
        return Promise.resolve(new AsyncIteratorRecord(data, false));
      }

      promise = new Promise(this[kHandlePromise]);
    }

    this[kLastPromise] = promise;

    return promise;
  }

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
        resolve(new AsyncIteratorRecord(null, true));
      });
    });
  }
};

module.exports = ReadableAsyncIterator;
