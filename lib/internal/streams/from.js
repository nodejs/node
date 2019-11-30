'use strict';

const {
  Symbol,
} = primordials;

const {
  ERR_INVALID_ARG_TYPE
} = require('internal/errors').codes;

function from(Readable, iterable, opts) {
  let iterator;
  if (iterable && iterable[Symbol.asyncIterator])
    iterator = iterable[Symbol.asyncIterator]();
  else if (iterable && iterable[Symbol.iterator])
    iterator = iterable[Symbol.iterator]();
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
