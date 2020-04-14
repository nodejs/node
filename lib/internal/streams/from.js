'use strict';

const {
  SymbolAsyncIterator,
  SymbolIterator,
  Promise
} = primordials;
const { Buffer } = require('buffer');

const {
  ERR_INVALID_ARG_TYPE
} = require('internal/errors').codes;

function from(Readable, iterable, opts) {
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

  let onDataNeeded;

  const readable = new Readable({
    objectMode: true,
    ...opts,
    read() {
      onDataNeeded && onDataNeeded();
    },
    async destroy(error, cb) {
      onDataNeeded && onDataNeeded();
      try {
        await pumping;
      } catch (e) {
        // Do not hide present error
        if (!error) error = e;
      }
      cb(error);
    },
  });

  if (!iterable[SymbolAsyncIterator] && !iterable[SymbolIterator])
    throw new ERR_INVALID_ARG_TYPE('iterable', ['Iterable'], iterable);

  const pumping = pump();

  return readable;

  async function pump() {
    /*
    We're iterating over sync or async iterator with the appropriate sync
    or async version of the `for-of` loop.

    `for-await-of` loop has an edge case when looping over synchronous
    iterator.

    It does not close synchronous iterator with .return() if that iterator
    yields rejected Promise, so finally blocks within such an iterator are
    never executed.

    In the application code developers can choose between async and sync
    forms of the loop depending on their needs, but in the library code we
    have to handle such edge cases properly and close iterators anyway.
    */
    try {
      if (iterable[SymbolAsyncIterator]) {
        for await (const data of iterable) {
          if (readable.destroyed) return;
          if (!readable.push(data)) {
            await new Promise((resolve) => { onDataNeeded = resolve; });
            if (readable.destroyed) return;
          }
        }
      } else {
        for (const data of iterable) {
          const value = await data;
          if (readable.destroyed) return;
          if (!readable.push(value)) {
            await new Promise((resolve) => { onDataNeeded = resolve; });
            if (readable.destroyed) return;
          }
        }
      }
      if (!readable.destroyed) readable.push(null);
    } catch (error) {
      if (!readable.destroyed) readable.destroy(error);
    }
  }
}

module.exports = from;
