'use strict';

const common = require('../common');
const assert = require('assert');
const { PerformanceObserver, performance } = require('perf_hooks');

// Populate beyond the lazy-indexing threshold with both unique and duplicate names.
{
  const count = 100;
  for (let i = 0; i < count; i++) {
    performance.mark(`m-${i}`, { startTime: (count - i) * 10 });
    performance.measure(`m-${i}`, { start: (count - i) * 10 + 1, duration: 5 });
  }
  // Add duplicates with out-of-order startTime.
  performance.mark('m-10', { startTime: 5 });
  performance.mark('m-10', { startTime: 5000 });
  performance.measure('m-10', { start: 3, duration: 1 });

  // Single-entry lookup by name and type.
  const singleMark = performance.getEntriesByName('m-0', 'mark');
  assert.strictEqual(singleMark.length, 1);
  assert.strictEqual(singleMark[0].name, 'm-0');
  assert.strictEqual(singleMark[0].entryType, 'mark');
  assert.strictEqual(singleMark[0].startTime, count * 10);

  // Mutating the returned array must not affect the internal buffer.
  singleMark.pop();
  assert.strictEqual(performance.getEntriesByName('m-0', 'mark').length, 1);

  // Multi-entry lookup must be sorted by startTime and isolated from mutation.
  const dupMarks = performance.getEntriesByName('m-10', 'mark');
  assert.deepStrictEqual(dupMarks.map((e) => e.startTime), [5, 900, 5000]);
  dupMarks.pop();
  assert.strictEqual(performance.getEntriesByName('m-10', 'mark').length, 3);

  // Combined getEntriesByName without type includes both marks and measures sorted by startTime.
  const combined = performance.getEntriesByName('m-10');
  assert.deepStrictEqual(
    combined.map((e) => [e.entryType, e.startTime]),
    [
      ['measure', 3],
      ['mark', 5],
      ['mark', 900],
      ['measure', 901],
      ['mark', 5000],
    ],
  );

  // Unknown name and unknown type return empty arrays.
  assert.deepStrictEqual(performance.getEntriesByName('does-not-exist'), []);
  assert.deepStrictEqual(performance.getEntriesByName('m-10', 'invalid'), []);

  // clearMarks(name) removes only that mark name (including duplicates), not measures.
  performance.clearMarks('m-10');
  performance.clearMarks('does-not-exist');
  assert.strictEqual(performance.getEntriesByName('m-10', 'mark').length, 0);
  assert.strictEqual(performance.getEntriesByName('m-10', 'measure').length, 2);
  assert.strictEqual(performance.getEntriesByType('mark').length, count - 1);

  // clearMeasures(name) removes only that measure name.
  performance.clearMeasures('m-10');
  assert.strictEqual(performance.getEntriesByName('m-10', 'measure').length, 0);
  assert.strictEqual(performance.getEntriesByName('m-10').length, 0);
  assert.strictEqual(performance.getEntriesByType('measure').length, count - 1);

  // Pushing new entries after indexing is active works and maintains sort order.
  performance.mark('m-10', { startTime: 42 });
  assert.deepStrictEqual(
    performance.getEntriesByName('m-10', 'mark').map((e) => e.startTime),
    [42],
  );

  // getEntriesByType / getEntries return sorted arrays.
  const allMarks = performance.getEntriesByType('mark');
  assert.strictEqual(allMarks.length, count);
  for (let i = 1; i < allMarks.length; i++) {
    assert.ok(allMarks[i - 1].startTime <= allMarks[i].startTime);
  }
  const allEntries = performance.getEntries();
  assert.strictEqual(allEntries.length, count + count - 1);
  for (let i = 1; i < allEntries.length; i++) {
    assert.ok(allEntries[i - 1].startTime <= allEntries[i].startTime);
  }

  // PerformanceObserver with buffered: true reads from the indexed buffer.
  const obs = new PerformanceObserver(common.mustCall((list) => {
    assert.strictEqual(list.getEntries().length, count);
    obs.disconnect();
    testFrozenAndNoHiddenProperties();
  }));
  obs.observe({ type: 'mark', buffered: true });

  performance.clearMarks();
  performance.clearMeasures();
  assert.strictEqual(performance.getEntries().length, 0);
}

// Frozen/sealed entries must work across indexing, and indexing must not attach
// own symbol properties to entries.
function testFrozenAndNoHiddenProperties() {
  performance.clearMarks();
  performance.clearMeasures();

  const frozenMark = Object.freeze(performance.mark('frozen', { startTime: 10 }));
  const sealedMeasure = Object.seal(performance.measure('sealed', { start: 10, duration: 1 }));
  const beforeIndexMark = performance.mark('eq-test', { startTime: 123 });
  const symbolsBefore = Object.getOwnPropertySymbols(frozenMark);

  // Populate past 128 entries so clearing > half triggers compaction.
  for (let i = 0; i < 140; i++) {
    performance.mark(`f-${i}`, { startTime: 20 + i });
    performance.measure(`mf-${i}`, { start: 20 + i, duration: 1 });
  }

  // Trigger indexing (2 per-name lookups) on buffers containing frozen/sealed entries.
  assert.strictEqual(performance.getEntriesByName('frozen', 'mark')[0], frozenMark);
  assert.strictEqual(performance.getEntriesByName('frozen', 'mark')[0], frozenMark);
  assert.strictEqual(performance.getEntriesByName('sealed', 'measure')[0], sealedMeasure);
  assert.strictEqual(performance.getEntriesByName('sealed', 'measure')[0], sealedMeasure);
  assert.deepStrictEqual(Object.getOwnPropertySymbols(frozenMark), symbolsBefore);

  // Identical marks created before and after indexing must remain deepStrictEqual.
  const afterIndexMark = performance.mark('eq-test', { startTime: 123 });
  assert.deepStrictEqual(beforeIndexMark, afterIndexMark);

  // Clearing frozen/sealed entries and exercising the compaction path (>= 128 slots, dead >= size).
  performance.clearMarks('frozen');
  performance.clearMarks('eq-test');
  performance.clearMeasures('sealed');
  assert.strictEqual(performance.getEntriesByName('frozen', 'mark').length, 0);
  assert.strictEqual(performance.getEntriesByName('sealed', 'measure').length, 0);

  for (let i = 0; i < 80; i++) {
    performance.clearMarks(`f-${i}`);
  }
  assert.strictEqual(performance.getEntriesByType('mark').length, 60);
  assert.strictEqual(performance.getEntriesByName('f-139', 'mark').length, 1);

  // Draining an indexed buffer completely via clearMarks(name) resets it cleanly.
  for (let i = 80; i < 140; i++) {
    performance.clearMarks(`f-${i}`);
  }
  assert.strictEqual(performance.getEntriesByType('mark').length, 0);
  performance.mark('after-drain', { startTime: 1 });
  assert.strictEqual(performance.getEntriesByName('after-drain', 'mark').length, 1);

  // Overriding PerformanceEntry.prototype.name must not trigger user getters or
  // corrupt lookups/clears across unindexed, indexed, and incremental-sync paths.
  const origDesc = Object.getOwnPropertyDescriptor(PerformanceEntry.prototype, 'name');
  {
    performance.clearMarks();
    for (let i = 0; i < 70; i++) {
      performance.mark(`re-${i}`);
    }
    Object.defineProperty(PerformanceEntry.prototype, 'name', {
      configurable: true,
      get() {
        throw new Error('getter-should-not-be-called');
      },
    });
    try {
      // Unindexed first lookup, indexed second lookup, incremental sync, and clearMarks(name).
      assert.strictEqual(performance.getEntriesByName('re-0', 'mark').length, 1);
      assert.strictEqual(performance.getEntriesByName('re-0', 'mark').length, 1);
      performance.mark('tail-a');
      assert.strictEqual(performance.getEntriesByName('tail-a', 'mark').length, 1);
      performance.clearMarks('re-2');
      assert.deepStrictEqual(performance.getEntriesByName('re-2', 'mark'), []);
      assert.strictEqual(performance.getEntriesByType('mark').length, 70);
    } finally {
      Object.defineProperty(PerformanceEntry.prototype, 'name', origDesc);
    }
  }

  performance.clearMarks();
  performance.clearMeasures();

  testEqualStartTimeOrder();
}

// Entries with identical startTime must preserve insertion order after indexing.
function testEqualStartTimeOrder() {
  performance.clearMarks();
  performance.clearMeasures();

  for (let i = 0; i < 70; i++) {
    performance.mark(`pad-${i}`, { startTime: 1000 + i });
    performance.measure(`mpad-${i}`, { start: 1000 + i, duration: 1 });
  }
  // Trigger indexing on both buffers (2 per-name lookups).
  assert.strictEqual(performance.getEntriesByName('pad-0', 'mark').length, 1);
  assert.strictEqual(performance.getEntriesByName('pad-0', 'mark').length, 1);
  assert.strictEqual(performance.getEntriesByName('mpad-0', 'measure').length, 1);
  assert.strictEqual(performance.getEntriesByName('mpad-0', 'measure').length, 1);

  // Add interleaved entries with identical startTime (0 for measures without start).
  performance.mark('a', { startTime: 50 });
  performance.mark('b', { startTime: 50 });
  performance.mark('a', { startTime: 50 });

  performance.measure('ma');
  performance.measure('mb');
  performance.measure('ma');

  assert.deepStrictEqual(
    performance.getEntriesByType('mark').slice(0, 3).map((e) => e.name),
    ['a', 'b', 'a'],
  );
  assert.deepStrictEqual(
    performance.getEntriesByType('measure').slice(0, 3).map((e) => e.name),
    ['ma', 'mb', 'ma'],
  );
  assert.deepStrictEqual(
    performance.getEntries().slice(0, 6).map((e) => e.name),
    ['ma', 'mb', 'ma', 'a', 'b', 'a'],
  );

  const obs = new PerformanceObserver(common.mustCall((list) => {
    assert.deepStrictEqual(
      list.getEntries().slice(0, 3).map((e) => e.name),
      ['a', 'b', 'a'],
    );
    obs.disconnect();
    performance.clearMarks();
    performance.clearMeasures();
  }));
  obs.observe({ type: 'mark', buffered: true });
}
