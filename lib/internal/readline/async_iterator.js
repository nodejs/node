'use strict';

const kReadlineInterface = Symbol('readlineInterface');
const {
  kLastResolve,
  kLastReject,
  kError,
  kEnded,
  kLastPromise,
  kHandlePromise,
  kStream,
  onEnd,
  onError,
  wrapForNext,
  AsyncIteratorRecord,
  BaseAsyncIterator
} = require('internal/streams/base_async_iterator');

function readAndResolve(iter) {
  const resolve = iter[kLastResolve];
  if (resolve !== null) {
    const data = iter[kReadlineInterface].read();
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

function writeToBuffer(iter) {
  const data = iter[kStream].read();
  if (data !== null) {
    iter[kReadlineInterface]._normalWrite(data);
  }
  process.nextTick(readAndResolve, iter);
}

function onReadable(iter) {
  process.nextTick(writeToBuffer, iter);
}

const ReadlineAsyncIterator =
  class ReadlineAsyncIterator extends BaseAsyncIterator {
    constructor(readline_interface) {
      super();
      this[kReadlineInterface] = readline_interface;
      this[kStream] = readline_interface.input;
      this[kLastResolve] = null;
      this[kLastReject] = null;
      this[kError] = null;
      this[kEnded] = false;
      this[kLastPromise] = null;

      this[kStream].on('readable', onReadable.bind(null, this));
      this[kStream].on('end', onEnd.bind(null, this));
      this[kStream].on('error', onError.bind(null, this));

      // the function passed to new Promise
      // is cached so we avoid allocating a new
      // closure at every run
      this[kHandlePromise] = (resolve, reject) => {
        const data = this[kReadlineInterface].read();
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
        const data = this[kReadlineInterface].read();
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

module.exports = ReadlineAsyncIterator;
