'use strict';

require('../common');
const assert = require('node:assert');

for (const size of [0, 1, 2, 49, 50, 51, 63, 64, 65, 127, 128, 129,
                    255, 256, 257, 1023, 1024, 1025]) {
  const ascending = Array.from({ length: size }, (_, index) => [
    String(index).padStart(8, '0'), String(index),
  ]);
  const descending = ascending.slice().reverse();
  const duplicates = ascending.map(([, value], index) => [
    String(index % 7), value,
  ]);
  const shuffled = ascending.slice();
  let seed = 12345;
  for (let index = size - 1; index > 0; index--) {
    seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
    const other = seed % (index + 1);
    [shuffled[index], shuffled[other]] = [shuffled[other], shuffled[index]];
  }

  for (const entries of [ascending, descending, duplicates, shuffled]) {
    const expected = entries.slice().sort(([first], [second]) =>
      (first < second ? -1 : first > second ? 1 : 0));
    const params = new URLSearchParams(entries);
    const iterator = params.entries();
    params.toString();
    assert.strictEqual(params.sort(), undefined);
    assert.deepStrictEqual([...params], expected);
    assert.deepStrictEqual([...iterator], expected);
    assert.strictEqual(params.size, size);
    assert.strictEqual(params.toString(), new URLSearchParams(expected).toString());
    params.sort();
    assert.deepStrictEqual([...params], expected);
  }
}

{
  const entries = Array.from({ length: 65 }, (_, index) => [
    ['\u00e9', 'e\u0301', '\ud83d\ude00', '\uffff', ''][index % 5],
    `value ${index}`,
  ]);
  const expected = entries.slice().sort(([first], [second]) =>
    (first < second ? -1 : first > second ? 1 : 0));
  const url = new URL('https://example.org/');
  for (const [name, value] of entries) url.searchParams.append(name, value);
  assert.strictEqual(url.search, `?${new URLSearchParams(entries)}`);
  const params = url.searchParams;
  params.sort();
  assert.strictEqual(url.searchParams, params);
  assert.deepStrictEqual([...params], expected);
  assert.strictEqual(url.search, `?${new URLSearchParams(expected)}`);
}
