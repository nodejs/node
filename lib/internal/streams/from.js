'use strict';

const {
  SymbolAsyncIterator,
  SymbolIterator
} = primordials;
const { Buffer } = require('buffer');

const {
  ERR_INVALID_ARG_TYPE
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
    ...opts
  });
  // Reading boolean to protect against _read
  // being called before last iteration completion.
  let reading = false;
  readable._read = function() {
    if (!reading) {
      reading = true;
      next();
    }
  };
  async function next() {
    try {
      const { value, done } = await iterator.next();
      if (done) {
        readable.push(null);
      } else if (readable.push(await value)) {
        next();
      } else {
        reading = false;
      }
    } catch (err) {
      readable.destroy(err);
    }
  }
  return readable;
}

module.exports = from;
