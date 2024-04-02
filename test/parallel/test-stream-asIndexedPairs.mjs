import '../common/index.mjs';
import { Readable } from 'stream';
import { deepStrictEqual, rejects, throws } from 'assert';

{
  // asIndexedPairs with a synchronous stream
  const pairs = await Readable.from([1, 2, 3]).asIndexedPairs().toArray();
  deepStrictEqual(pairs, [[0, 1], [1, 2], [2, 3]]);
  const empty = await Readable.from([]).asIndexedPairs().toArray();
  deepStrictEqual(empty, []);
}

{
  // asIndexedPairs works an asynchronous streams
  const asyncFrom = (...args) => Readable.from(...args).map(async (x) => x);
  const pairs = await asyncFrom([1, 2, 3]).asIndexedPairs().toArray();
  deepStrictEqual(pairs, [[0, 1], [1, 2], [2, 3]]);
  const empty = await asyncFrom([]).asIndexedPairs().toArray();
  deepStrictEqual(empty, []);
}

{
  // Does not enumerate an infinite stream
  const infinite = () => Readable.from(async function* () {
    while (true) yield 1;
  }());
  const pairs = await infinite().asIndexedPairs().take(3).toArray();
  deepStrictEqual(pairs, [[0, 1], [1, 1], [2, 1]]);
  const empty = await infinite().asIndexedPairs().take(0).toArray();
  deepStrictEqual(empty, []);
}

{
  // AbortSignal
  await rejects(async () => {
    const ac = new AbortController();
    const { signal } = ac;
    const p = Readable.from([1, 2, 3]).asIndexedPairs({ signal }).toArray();
    ac.abort();
    await p;
  }, { name: 'AbortError' });

  await rejects(async () => {
    const signal = AbortSignal.abort();
    await Readable.from([1, 2, 3]).asIndexedPairs({ signal }).toArray();
  }, /AbortError/);
}

{
  // Error cases
  throws(() => Readable.from([1]).asIndexedPairs(1), /ERR_INVALID_ARG_TYPE/);
  throws(() => Readable.from([1]).asIndexedPairs({ signal: true }), /ERR_INVALID_ARG_TYPE/);
}
