'use strict';

const {
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
  AsyncIteratorRecord,
  BaseAsyncIterator
} = require('internal/streams/base_async_iterator');

const ReadableAsyncIterator =
  class ReadableAsyncIterator extends BaseAsyncIterator {
    constructor(stream) {
      super();
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
