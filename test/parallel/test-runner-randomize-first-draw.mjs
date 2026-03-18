import '../common/index.mjs';
import assert from 'node:assert';
import { test, run } from 'node:test';
import * as fixtures from '../common/fixtures.mjs';

const fixture = fixtures.path('test-runner', 'randomize', 'first-draw.cjs');
const cases = [
  { seed: 0, expected: ['a', 'b', 'c'] },
  { seed: 1, expected: ['b', 'a', 'c'] },
  { seed: 2, expected: ['c', 'a', 'b'] },
];

test('randomizes the first subtest draw deterministically', async () => {
  for (const { seed, expected } of cases) {
    const actual = [];
    const stream = run({
      files: [fixture],
      isolation: 'process',
      randomSeed: seed,
    });

    stream.on('test:fail', ({ details }) => {
      throw details.error;
    });

    stream.on('test:pass', ({ name }) => {
      if (name === 'a' || name === 'b' || name === 'c') {
        actual.push(name);
      }
    });

    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);

    assert.deepStrictEqual(actual, expected, `seed ${seed}`);
  }
});
