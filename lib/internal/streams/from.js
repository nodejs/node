'use strict';

const {
  PromisePrototypeThen,
  SymbolAsyncIterator,
  SymbolIterator
} = primordials;
const { Buffer } = require('buffer');

const {
  ERR_INVALID_ARG_TYPE,
  ERR_STREAM_NULL_VALUES
} = require('internal/errors').codes;

function from(Readable, iterable, opts) {
  let iterator;
  if (typeof iterable === 'string' || iterable instanceof Buffer) {
    return new Readable({
      objectMode: true,
      ...opts,
      read() {
        this.push(iterable);
        this.push(null);
      }
    });
  }

  if (iterable && iterable[SymbolAsyncIterator])
    iterator = iterable[SymbolAsyncIterator]();
  else if (iterable && iterable[SymbolIterator])
    iterator = iterable[SymbolIterator]();
  else
    throw new ERR_INVALID_ARG_TYPE('iterable', ['Iterable'], iterable);

  const readable = new Readable({
    objectMode: true,
    highWaterMark: 1,
    // TODO(ronag): What options should be allowed?
    ...opts
  });

  // Flag to protect against _read
  // being called before last iteration completion.
  let reading = false;

  readable._read = function() {
    if (!reading) {
      reading = true;
      next();
    }
  };

  readable._destroy = function(error, cb) {
    PromisePrototypeThen(
      close(error),
      () => process.nextTick(cb, error), // nextTick is here in case cb throws
      (e) => process.nextTick(cb, e || error),
    );
  };

  async function close(error) {
    const hadError = (error !== undefined) && (error !== null);
    const hasThrow = typeof iterator.throw === 'function';
    if (hadError && hasThrow) {
      const { value, done } = await iterator.throw(error);
      await value;
      if (done) {
        return;
      }
    }
    if (typeof iterator.return === 'function') {
      const { value } = await iterator.return();
      await value;
    }
  }

  async function next() {
    try {
      const { value, done } = await iterator.next();
      if (done) {
        readable.push(null);
      } else {
        const res = await value;
        if (res === null) {
          reading = false;
          throw new ERR_STREAM_NULL_VALUES();
        } else if (readable.push(res)) {
          next();
        } else {
          reading = false;
        }
      }
    } catch (err) {
      readable.destroy(err);
    }
  }
  return readable;
}

module.exports = from;
