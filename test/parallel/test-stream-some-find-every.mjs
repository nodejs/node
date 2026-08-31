import * as common from '../common/index.mjs';
import { setTimeout } from 'timers/promises';
import { Readable } from 'stream';
import assert from 'assert';


function oneTo5() {
  return Readable.from([1, 2, 3, 4, 5]);
}

function oneTo5Async() {
  return oneTo5().map(async (x) => {
    await Promise.resolve();
    return x;
  });
}
{
  // Some, find, and every work with a synchronous stream and predicate
  assert.strictEqual(await oneTo5().some((x) => x > 3), true);
  assert.strictEqual(await oneTo5().every((x) => x > 3), false);
  assert.strictEqual(await oneTo5().find((x) => x > 3), 4);
  assert.strictEqual(await oneTo5().some((x) => x > 6), false);
  assert.strictEqual(await oneTo5().every((x) => x < 6), true);
  assert.strictEqual(await oneTo5().find((x) => x > 6), undefined);
  assert.strictEqual(await Readable.from([]).some(() => true), false);
  assert.strictEqual(await Readable.from([]).every(() => true), true);
  assert.strictEqual(await Readable.from([]).find(() => true), undefined);
}

{
  // Some, find, and every work with an asynchronous stream and synchronous predicate
  assert.strictEqual(await oneTo5Async().some((x) => x > 3), true);
  assert.strictEqual(await oneTo5Async().every((x) => x > 3), false);
  assert.strictEqual(await oneTo5Async().find((x) => x > 3), 4);
  assert.strictEqual(await oneTo5Async().some((x) => x > 6), false);
  assert.strictEqual(await oneTo5Async().every((x) => x < 6), true);
  assert.strictEqual(await oneTo5Async().find((x) => x > 6), undefined);
}

{
  // Some, find, and every work on synchronous streams with an asynchronous predicate
  assert.strictEqual(await oneTo5().some(async (x) => x > 3), true);
  assert.strictEqual(await oneTo5().every(async (x) => x > 3), false);
  assert.strictEqual(await oneTo5().find(async (x) => x > 3), 4);
  assert.strictEqual(await oneTo5().some(async (x) => x > 6), false);
  assert.strictEqual(await oneTo5().every(async (x) => x < 6), true);
  assert.strictEqual(await oneTo5().find(async (x) => x > 6), undefined);
}

{
  // Some, find, and every await thenable predicates.
  const expected = new Map([
    ['some', false],
    ['every', false],
    ['find', undefined],
  ]);

  for (const op of expected.keys()) {
    const thenable = {
      then(resolve) {
        thenable.receiver = this;
        resolve(false);
      },
    };
    assert.strictEqual(
      await Readable.from([1])[op](() => thenable),
      expected.get(op),
    );
    assert.strictEqual(thenable.receiver, thenable);
  }
}

{
  // Predicates receive an options object containing an AbortSignal.
  for (const op of ['some', 'every', 'find']) {
    let predicateSignal;
    await Readable.from([1])[op](common.mustCall((value, { signal }) => {
      assert.strictEqual(value, 1);
      assert.ok(signal instanceof AbortSignal);
      predicateSignal = signal;
      return true;
    }));
    assert.strictEqual(predicateSignal.aborted, true);
  }
}

{
  // Some, find, and every work on asynchronous streams with an asynchronous predicate
  assert.strictEqual(await oneTo5Async().some(async (x) => x > 3), true);
  assert.strictEqual(await oneTo5Async().every(async (x) => x > 3), false);
  assert.strictEqual(await oneTo5Async().find(async (x) => x > 3), 4);
  assert.strictEqual(await oneTo5Async().some(async (x) => x > 6), false);
  assert.strictEqual(await oneTo5Async().every(async (x) => x < 6), true);
  assert.strictEqual(await oneTo5Async().find(async (x) => x > 6), undefined);
}

{
  async function checkDestroyed(stream) {
    await setTimeout();
    assert.strictEqual(stream.destroyed, true);
  }

  {
    // Some, find, and every short circuit
    const someStream = oneTo5();
    await someStream.some(common.mustCall((x) => x > 2, 3));
    await checkDestroyed(someStream);

    const everyStream = oneTo5();
    await everyStream.every(common.mustCall((x) => x < 3, 3));
    await checkDestroyed(everyStream);

    const findStream = oneTo5();
    await findStream.find(common.mustCall((x) => x > 1, 2));
    await checkDestroyed(findStream);

    // When short circuit isn't possible the whole stream is iterated
    await oneTo5().some(common.mustCall(() => false, 5));
    await oneTo5().every(common.mustCall(() => true, 5));
    await oneTo5().find(common.mustCall(() => false, 5));
  }

  {
    // Some, find, and every short circuit async stream/predicate
    const someStream = oneTo5Async();
    await someStream.some(common.mustCall(async (x) => x > 2, 3));
    await checkDestroyed(someStream);

    const everyStream = oneTo5Async();
    await everyStream.every(common.mustCall(async (x) => x < 3, 3));
    await checkDestroyed(everyStream);

    const findStream = oneTo5Async();
    await findStream.find(common.mustCall(async (x) => x > 1, 2));
    await checkDestroyed(findStream);

    // When short circuit isn't possible the whole stream is iterated
    await oneTo5Async().some(common.mustCall(async () => false, 5));
    await oneTo5Async().every(common.mustCall(async () => true, 5));
    await oneTo5Async().find(common.mustCall(async () => false, 5));
  }
}

{
  // Concurrency doesn't affect which value is found.
  const found = await Readable.from([1, 2]).find(async (val) => {
    if (val === 1) {
      await setTimeout(100);
    }
    return true;
  }, { concurrency: 2 });
  assert.strictEqual(found, 1);
}

{
  // Errors from any active predicate take precedence over a concurrent match.
  const first = Promise.withResolvers();
  const second = Promise.withResolvers();
  const error = new Error('boom');
  const result = Readable.from([1, 2]).find(common.mustCall((val) => {
    return val === 1 ? first.promise : second.promise;
  }, 2), { concurrency: 2 });

  second.resolve(true);
  await setTimeout();
  first.reject(error);
  await assert.rejects(result, error);
}

{
  // Synchronous throws and rejected promises from predicates are propagated.
  for (const op of ['some', 'every', 'find']) {
    const syncError = new Error(`${op} sync`);
    const syncStream = oneTo5();
    await assert.rejects(syncStream[op](common.mustCall(() => {
      throw syncError;
    }, 1)), syncError);
    assert.strictEqual(syncStream.destroyed, true);

    const asyncError = new Error(`${op} async`);
    const asyncStream = oneTo5();
    await assert.rejects(asyncStream[op](common.mustCall(async () => {
      throw asyncError;
    }, 1)), asyncError);
    assert.strictEqual(asyncStream.destroyed, true);
  }
}

{
  // Find can leave the source open after a match.
  const stream = oneTo5();
  const found = await stream.find((x) => x === 2, {
    destroyOnReturn: false,
  });
  assert.strictEqual(found, 2);
  assert.strictEqual(stream.destroyed, false);
  assert.strictEqual(stream.read(), 3);
  stream.destroy();
}

{
  // Support for AbortSignal
  for (const op of ['some', 'every', 'find']) {
    {
      const ac = new AbortController();
      assert.rejects(Readable.from([1, 2, 3])[op](
        () => new Promise(() => { }),
        { signal: ac.signal }
      ), {
        name: 'AbortError',
      }, `${op} should abort correctly with sync abort`).then(common.mustCall());
      ac.abort();
    }
    {
      // Support for pre-aborted AbortSignal
      assert.rejects(Readable.from([1, 2, 3])[op](
        () => new Promise(() => { }),
        { signal: AbortSignal.abort() }
      ), {
        name: 'AbortError',
      }, `${op} should abort with pre-aborted abort controller`).then(common.mustCall());
    }
  }
}
{
  // Error cases
  for (const op of ['some', 'every', 'find']) {
    assert.rejects(async () => {
      await Readable.from([1])[op](1);
    }, /ERR_INVALID_ARG_TYPE/, `${op} should throw for invalid function`).then(common.mustCall());
    assert.rejects(async () => {
      await Readable.from([1])[op]((x) => x, {
        concurrency: 'Foo'
      });
    }, /ERR_OUT_OF_RANGE/, `${op} should throw for invalid concurrency`).then(common.mustCall());
    assert.rejects(async () => {
      await Readable.from([1])[op]((x) => x, 1);
    }, /ERR_INVALID_ARG_TYPE/, `${op} should throw for invalid concurrency`).then(common.mustCall());
    assert.rejects(async () => {
      await Readable.from([1])[op]((x) => x, {
        signal: true
      });
    }, /ERR_INVALID_ARG_TYPE/, `${op} should throw for invalid signal`).then(common.mustCall());
    assert.rejects(async () => {
      await Readable.from([1])[op]((x) => x, {
        destroyOnReturn: 'false'
      });
    }, /ERR_INVALID_ARG_TYPE/, `${op} should throw for invalid destroyOnReturn`).then(common.mustCall());
  }
}
{
  for (const op of ['some', 'every', 'find']) {
    const stream = oneTo5();
    Object.defineProperty(stream, 'map', {
      value: common.mustNotCall(),
    });
    // Check that map isn't getting called.
    stream[op](() => {});
  }
}
