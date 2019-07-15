'use strict';
const { mustCall, expectsError } = require('../common');
const { once } = require('events');
const { Readable, Transform } = require('stream');
const { strictEqual } = require('assert');

async function transformBy() {
  const readable = Readable.from('test');
  async function * mapper(source) {
    for await (const chunk of source) {
      yield chunk.toUpperCase();
    }
  }

  const stream = Transform.by(mapper);
  readable.pipe(stream);
  const expected = ['T', 'E', 'S', 'T'];
  for await (const chunk of stream) {
    strictEqual(chunk, expected.shift());
  }
}

async function transformByFuncReturnsObjectWithSymbolAsyncIterator() {
  const readable = Readable.from('test');
  const mapper = (source) => ({
    [Symbol.asyncIterator]() {
      return {
        async next() {
          const { done, value } = await source.next();
          return { done, value: value ? value.toUpperCase() : value };
        }
      };
    }
  });

  const stream = Transform.by(mapper);
  readable.pipe(stream);
  const expected = ['T', 'E', 'S', 'T'];
  for await (const chunk of stream) {
    strictEqual(chunk, expected.shift());
  }
}

async function
transformByObjReturnedWSymbolAsyncIteratorWithNonPromiseReturningNext() {
  const mapper = (source) => ({
    [Symbol.asyncIterator]() {
      return {
        next() {
          const { done, value } = source.next();
          return { done, value: value ? value.toUpperCase() : value };
        }
      };
    }
  });

  expectsError(() => Transform.by(mapper), {
    message: 'asyncGeneratorFn must return an async iterable',
    code: 'ERR_ARG_RETURN_VALUE_NOT_ASYNC_ITERABLE',
    type: TypeError
  });
}

async function transformByObjReturnedWSymbolAsyncIteratorWithNoNext() {
  const mapper = () => ({
    [Symbol.asyncIterator]() {
      return {};
    }
  });

  expectsError(() => Transform.by(mapper), {
    message: 'asyncGeneratorFn must return an async iterable',
    code: 'ERR_ARG_RETURN_VALUE_NOT_ASYNC_ITERABLE',
    type: TypeError
  });
}

async function transformByObjReturnedWSymbolAsyncIteratorThatIsNotFunction() {
  const mapper = () => ({
    [Symbol.asyncIterator]: 'wrong'
  });

  expectsError(() => Transform.by(mapper), {
    message: 'asyncGeneratorFn must return an async iterable',
    code: 'ERR_ARG_RETURN_VALUE_NOT_ASYNC_ITERABLE',
    type: TypeError
  });
}

async function transformByFuncReturnsObjectWithoutSymbolAsyncIterator() {
  const mapper = () => ({});

  expectsError(() => Transform.by(mapper), {
    message: 'asyncGeneratorFn must return an async iterable',
    code: 'ERR_ARG_RETURN_VALUE_NOT_ASYNC_ITERABLE',
    type: TypeError
  });
}

async function transformByEncoding() {
  const readable = Readable.from('test');
  async function * mapper(source) {
    for await (const chunk of source) {
      strictEqual(source.encoding, 'ascii');
      yield chunk.toUpperCase();
    }
  }
  const stream = Transform.by(mapper);
  stream.setDefaultEncoding('ascii');
  readable.pipe(stream);

  const expected = ['T', 'E', 'S', 'T'];
  for await (const chunk of stream) {
    strictEqual(chunk, expected.shift());
  }
}

async function transformBySourceIteratorCompletes() {
  const readable = Readable.from('test');
  const mustReach = mustCall();
  async function * mapper(source) {
    for await (const chunk of source) {
      yield chunk.toUpperCase();
    }
    mustReach();
  }

  const stream = Transform.by(mapper);
  readable.pipe(stream);
  const expected = ['T', 'E', 'S', 'T'];
  for await (const chunk of stream) {
    strictEqual(chunk, expected.shift());
  }
}

async function transformByYieldPlusReturn() {
  const readable = Readable.from('test');
  async function * mapper(source) {
    for await (const chunk of source) {
      yield chunk.toUpperCase();
    }
    return 'final chunk';
  }

  const stream = Transform.by(mapper);
  readable.pipe(stream);
  const expected = ['T', 'E', 'S', 'T', 'final chunk'];
  for await (const chunk of stream) {
    strictEqual(chunk, expected.shift());
  }
}

async function transformByReturnEndsStream() {
  const readable = Readable.from('test');
  async function * mapper(source) {
    for await (const chunk of source) {
      yield chunk.toUpperCase();
      return 'stop';
    }
  }

  const stream = Transform.by(mapper);
  readable.pipe(stream);
  const expected = ['T', 'stop'];
  const mustReach = mustCall();
  for await (const chunk of stream) {
    strictEqual(chunk, expected.shift());
  }
  mustReach();
}

async function transformByOnData() {
  const readable = Readable.from('test');
  async function * mapper(source) {
    for await (const chunk of source) {
      yield chunk.toUpperCase();
    }
  }

  const stream = Transform.by(mapper);
  readable.pipe(stream);
  const expected = ['T', 'E', 'S', 'T'];
  let iterations = 0;
  stream.on('data', (chunk) => {
    iterations++;
    strictEqual(chunk, expected.shift());
  });

  await once(stream, 'end');
  strictEqual(iterations, 4);
}

async function transformByOnDataNonObject() {
  const readable = Readable.from('test', { objectMode: false });
  async function * mapper(source) {
    for await (const chunk of source) {
      yield chunk.toString().toUpperCase();
    }
  }
  const stream = Transform.by(mapper, { objectMode: false });
  readable.pipe(stream);
  const expected = ['T', 'E', 'S', 'T'];
  let iterations = 0;
  stream.on('data', (chunk) => {
    iterations++;
    strictEqual(chunk instanceof Buffer, true);
    strictEqual(chunk.toString(), expected.shift());
  });

  await once(stream, 'end');
  strictEqual(iterations, 4);
}

async function transformByOnErrorAndDestroyed() {
  const stream = Readable.from('test').pipe(Transform.by(
    async function * mapper(source) {
      for await (const chunk of source) {
        if (chunk === 'e') throw new Error('kaboom');
        yield chunk.toUpperCase();
      }
    }
  ));
  stream.on('data', (chunk) => {
    strictEqual(chunk.toString(), 'T');
  });
  strictEqual(stream.destroyed, false);
  const [ err ] = await once(stream, 'error');
  strictEqual(err.message, 'kaboom');
  strictEqual(stream.destroyed, true);
}

async function transformByErrorTryCatchAndDestroyed() {
  const stream = Readable.from('test').pipe(Transform.by(
    async function * mapper(source) {
      for await (const chunk of source) {
        if (chunk === 'e') throw new Error('kaboom');
        yield chunk.toUpperCase();
      }
    }
  ));
  strictEqual(stream.destroyed, false);
  try {
    for await (const chunk of stream) {
      strictEqual(chunk.toString(), 'T');
    }
  } catch (err) {
    strictEqual(err.message, 'kaboom');
    strictEqual(stream.destroyed, true);
  }
}

async function transformByOnErrorAndTryCatchAndDestroyed() {
  const stream = Readable.from('test').pipe(Transform.by(
    async function * mapper(source) {
      for await (const chunk of source) {
        if (chunk === 'e') throw new Error('kaboom');
        yield chunk.toUpperCase();
      }
    }
  ));
  strictEqual(stream.destroyed, false);
  stream.once('error', mustCall((err) => {
    strictEqual(err.message, 'kaboom');
  }));
  try {
    for await (const chunk of stream) {
      strictEqual(chunk.toString(), 'T');
    }
  } catch (err) {
    strictEqual(err.message, 'kaboom');
    strictEqual(stream.destroyed, true);
  }
}

async function transformByThrowPriorToForAwait() {
  async function * generate() {
    yield 'a';
    yield 'b';
    yield 'c';
  }
  const read = Readable.from(generate());
  const stream = Transform.by(async function * transformA(source) {
    throw new Error('kaboom');
  });
  stream.on('error', mustCall((err) => {
    strictEqual(err.message, 'kaboom');
  }));

  read.pipe(stream);
}

Promise.all([
  transformBy(),
  transformByFuncReturnsObjectWithSymbolAsyncIterator(),
  transformByObjReturnedWSymbolAsyncIteratorWithNonPromiseReturningNext(),
  transformByObjReturnedWSymbolAsyncIteratorWithNoNext(),
  transformByObjReturnedWSymbolAsyncIteratorThatIsNotFunction(),
  transformByFuncReturnsObjectWithoutSymbolAsyncIterator(),
  transformByEncoding(),
  transformBySourceIteratorCompletes(),
  transformByYieldPlusReturn(),
  transformByReturnEndsStream(),
  transformByOnData(),
  transformByOnDataNonObject(),
  transformByOnErrorAndDestroyed(),
  transformByErrorTryCatchAndDestroyed(),
  transformByOnErrorAndTryCatchAndDestroyed(),
  transformByThrowPriorToForAwait()
]).then(mustCall());
