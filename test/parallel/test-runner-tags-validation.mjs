import { expectWarning } from '../common/index.mjs';
import assert from 'node:assert';
import { test, suite, describe, it, before, beforeEach, after, afterEach, run } from 'node:test';

// Silence the one-shot ExperimentalWarning that fires the first time tags are
// registered. Validation tests focus on the throw, not the warning.
expectWarning('ExperimentalWarning', 'Test tags is an experimental feature and might change at any time');

test('non-array tags throws ERR_INVALID_ARG_TYPE', () => {
  for (const bad of ['db', 42, {}, true, Symbol('s'), null]) {
    assert.throws(
      () => test('x', { tags: bad }, () => {}),
      { code: 'ERR_INVALID_ARG_TYPE' },
      `expected throw for tags=${String(bad)}`,
    );
  }
});

test('non-string element throws ERR_INVALID_ARG_TYPE', () => {
  for (const bad of [42, {}, true, null, undefined, Symbol('s')]) {
    assert.throws(
      () => test('x', { tags: ['db', bad] }, () => {}),
      { code: 'ERR_INVALID_ARG_TYPE' },
      `expected throw for element=${String(bad)}`,
    );
  }
});

test('empty-string tag throws ERR_INVALID_ARG_VALUE', () => {
  assert.throws(
    () => test('x', { tags: [''] }, () => {}),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
  assert.throws(
    () => test('x', { tags: ['db', ''] }, () => {}),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
});

test('Unicode and most punctuation are allowed', () => {
  // None of these should throw.
  test('unicode-1', { tags: ['café'] }, () => {});
  test('unicode-2', { tags: ['日本語'] }, () => {});
  test('punct-1', { tags: ['db:postgres'] }, () => {});
  test('punct-2', { tags: ['db-1'] }, () => {});
  test('punct-3', { tags: ['db.fast'] }, () => {});
  test('punct-4', { tags: ['db_fast'] }, () => {});
  test('punct-5', { tags: ['#critical'] }, () => {});
});

test('tags: [] is a no-op (does not throw)', () => {
  test('empty-tags', { tags: [] }, () => {});
});

test('case dedup: ["db", "DB", "Db"] collapses to single canonical entry', async (t) => {
  await t.test('child', { tags: ['db', 'DB', 'Db'] }, (ct) => {
    assert.deepStrictEqual(ct.tags, ['db']);
  });
});

test('declaration order is preserved on dedup', async (t) => {
  await t.test('child', { tags: ['Z', 'a', 'z', 'A'] }, (ct) => {
    assert.deepStrictEqual(ct.tags, ['z', 'a']);
  });
});

test('hooks silently ignore the tags option', () => {
  // Hooks must not throw if a caller mistakenly passes tags — the API has no
  // tags concept for hooks, but it should be tolerated. The validator only
  // runs on Test/Suite construction, not in the hook factory.
  before(() => {}, { tags: ['db'] });
  after(() => {}, { tags: ['db'] });
  beforeEach(() => {}, { tags: ['db'] });
  afterEach(() => {}, { tags: ['db'] });
});

test('suite() and describe() validate tags identically', () => {
  assert.throws(
    () => suite('s', { tags: 'db' }, () => {}),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.throws(
    () => describe('s', { tags: [''] }, () => {}),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
});

test('it() validates tags identically to test()', () => {
  assert.throws(
    () => it('i', { tags: [42] }, () => {}),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('run() rejects non-string testTagFilters values', () => {
  for (const bad of [[42], [{}], [null], [undefined], [true], [Symbol('s')]]) {
    assert.throws(
      () => run({ testTagFilters: bad }),
      { code: 'ERR_INVALID_ARG_TYPE' },
      `expected throw for testTagFilters=${JSON.stringify(bad)}`,
    );
  }
});

test('run() accepts a bare-string testTagFilters and normalizes it', async () => {
  // The string form is converted to a single-element array internally and
  // must not throw.
  const stream = run({ files: [], testTagFilters: 'db' });
  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
});

test('run() treats an empty testTagFilters array as a no-op', async () => {
  const stream = run({ files: [], testTagFilters: [] });
  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
});
