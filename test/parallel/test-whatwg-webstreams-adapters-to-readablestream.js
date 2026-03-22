// Flags: --no-warnings --expose-internals
'use strict';

const common = require('../common');

const assert = require('assert');
const { once } = require('events');

const {
  newReadableStreamFromStreamReadable,
} = require('internal/webstreams/adapters');

const {
  Duplex,
  PassThrough,
  Readable,
} = require('stream');

const {
  kState,
} = require('internal/webstreams/util');

{
  // Canceling the readableStream closes the readable.
  const readable = new Readable({
    read() {
      readable.push('hello');
      readable.push(null);
    }
  });

  readable.on('close', common.mustCall());
  readable.on('end', common.mustNotCall());
  readable.on('pause', common.mustCall());
  readable.on('resume', common.mustNotCall());
  readable.on('error', common.mustCall((error) => {
    assert.strictEqual(error.code, 'ABORT_ERR');
  }));

  const readableStream = newReadableStreamFromStreamReadable(readable);

  readableStream.cancel().then(common.mustCall());
}

{
  // Prematurely destroying the stream.Readable without an error
  // closes the ReadableStream with a premature close error but does
  // not error the readable.

  const readable = new Readable({
    read() {
      readable.push('hello');
      readable.push(null);
    }
  });

  const readableStream = newReadableStreamFromStreamReadable(readable);

  assert(!readableStream.locked);

  const reader = readableStream.getReader();

  assert.rejects(reader.closed, {
    code: 'ABORT_ERR',
  }).then(common.mustCall());

  readable.on('end', common.mustNotCall());
  readable.on('error', common.mustNotCall());

  readable.on('close', common.mustCall(() => {
    assert.strictEqual(readableStream[kState].state, 'errored');
  }));

  readable.destroy();
}

{
  // Ending the readable without an error just closes the
  // readableStream without an error.
  const readable = new Readable({
    read() {
      readable.push('hello');
      readable.push(null);
    }
  });

  const readableStream = newReadableStreamFromStreamReadable(readable);

  assert(!readableStream.locked);

  const reader = readableStream.getReader();

  reader.closed.then(common.mustCall());

  readable.on('end', common.mustCall());
  readable.on('error', common.mustNotCall());

  readable.on('close', common.mustCall(() => {
    assert.strictEqual(readableStream[kState].state, 'closed');
  }));

  readable.push(null);
}

{
  // Destroying the readable with an error should error the readableStream
  const error = new Error('boom');
  const readable = new Readable({
    read() {
      readable.push('hello');
      readable.push(null);
    }
  });

  const readableStream = newReadableStreamFromStreamReadable(readable);

  assert(!readableStream.locked);

  const reader = readableStream.getReader();

  assert.rejects(reader.closed, error).then(common.mustCall());

  readable.on('end', common.mustNotCall());
  readable.on('error', common.mustCall((reason) => {
    assert.strictEqual(reason, error);
  }));

  readable.on('close', common.mustCall(() => {
    assert.strictEqual(readableStream[kState].state, 'errored');
  }));

  readable.destroy(error);
}

{
  const readable = new Readable({
    encoding: 'utf8',
    read() {
      readable.push('hello');
      readable.push(null);
    }
  });

  const readableStream = newReadableStreamFromStreamReadable(readable);
  const reader = readableStream.getReader();

  readable.on('data', common.mustCall());
  readable.on('end', common.mustCall());
  readable.on('close', common.mustCall());

  (async () => {
    assert.deepStrictEqual(
      await reader.read(),
      { value: 'hello', done: false });
    assert.deepStrictEqual(
      await reader.read(),
      { value: undefined, done: true });

  })().then(common.mustCall());
}

{
  const data = {};
  const readable = new Readable({
    objectMode: true,
    read() {
      readable.push(data);
      readable.push(null);
    }
  });

  assert(readable.readableObjectMode);

  const readableStream = newReadableStreamFromStreamReadable(readable);
  const reader = readableStream.getReader();

  readable.on('data', common.mustCall());
  readable.on('end', common.mustCall());
  readable.on('close', common.mustCall());

  (async () => {
    assert.deepStrictEqual(
      await reader.read(),
      { value: data, done: false });
    assert.deepStrictEqual(
      await reader.read(),
      { value: undefined, done: true });

  })().then(common.mustCall());
}

{
  /**
   * Runs the same assertion across finalize-before/after and
   * default/BYOB adapter creation orders.
   * @param {(readable: Readable) => void | Promise<void>} finalize
   *   Finalizes the source stream before or after adaptation.
   * @param {(readAndAssert: () => Promise<void>, reader: ReadableStreamReader) => Promise<void>} postAssert
   *   Asserts the resulting web stream state for the current case.
   * @param {() => Readable} [createReadable]
   *   Creates the source stream for each case.
   */
  function testConfluence(finalize, postAssert, createReadable = () => new PassThrough()) {
    const cases = [false, true].flatMap((finalizeFirst) => [false, true].map((isBYOB) => ({ finalizeFirst, isBYOB })));
    Promise.all(cases.map(async (case_) => {
      try {
        const { isBYOB } = case_;
        const readable = createReadable();
        if (case_.finalizeFirst) {
          await finalize(readable);
        }
        /** @type {ReadableStream} */
        const readableStream = newReadableStreamFromStreamReadable(readable, { type: isBYOB ? 'bytes' : undefined });
        const reader = readableStream.getReader({ mode: isBYOB ? 'byob' : undefined });
        if (!case_.finalizeFirst) {
          await finalize(readable);
        }

        const readAndAssert = common.mustCall(() => {
          return reader.read(isBYOB ? new Uint8Array(1) : undefined).then((result) => {
            assert.deepStrictEqual(result, {
              value: isBYOB ? new Uint8Array(0) : undefined,
              done: true,
            });
          });
        });
        await postAssert(readAndAssert, reader);
      } catch (cause) {
        throw new Error(`Case failed: ${JSON.stringify(case_)}`, { cause });
      }
    })).then(common.mustCall());
  }
  const error = new Error('boom');
  // Ending the readable without an error => closes the readableStream without an error
  testConfluence(
    async (readable) => {
      readable.resume();
      readable.end();
      await once(readable, 'end');
    },
    common.mustCall(async (readAndAssert, reader) => {
      await readAndAssert();
      await reader.closed;
    }, 4)
  );
  // Prematurely destroying the stream.Readable without an error
  //   => errors the ReadableStream with a premature close error
  testConfluence(
    (readable) => readable.destroy(),
    common.mustCall(async (readAndAssert, reader) => {
      const errorPredicate = { code: 'ABORT_ERR' };
      await assert.rejects(readAndAssert(), errorPredicate);
      await assert.rejects(reader.closed, errorPredicate);
    }, 4)
  );
  // Asynchronously destroyed readable => errors the ReadableStream with a
  // premature close error regardless of adapter creation order.
  class AsyncDestroyReadable extends Readable {
    _read() {}

    _destroy(error, callback) {
      setImmediate(callback, error);
    }
  }
  testConfluence(
    (readable) => readable.destroy(),
    common.mustCall(async (readAndAssert, reader) => {
      const errorPredicate = { code: 'ABORT_ERR' };
      await assert.rejects(readAndAssert(), errorPredicate);
      await assert.rejects(reader.closed, errorPredicate);
    }, 4),
    () => new AsyncDestroyReadable()
  );
  // Destroying the readable with an error => errors the readableStream
  testConfluence(
    common.mustCall((readable) => {
      readable.on('error', common.mustCall((reason) => {
        assert.strictEqual(reason, error);
      }));
      readable.destroy(error);
    }, 4),
    common.mustCall(async (readAndAssert, reader) => {
      await assert.rejects(readAndAssert(), error);
      await assert.rejects(reader.closed, error);
    }, 4)
  );
}

{
  const duplex = new Duplex({ readable: false });
  duplex.destroy();
  const readableStream = newReadableStreamFromStreamReadable(duplex);
  const reader = readableStream.getReader();
  reader.closed.then(common.mustCall());
}
