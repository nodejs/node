// Flags: --expose-internals
'use strict';

// Checks how time profiles produced by the pprof binding are normalized and
// serialized, with synthetic profiles.

require('../common');
const assert = require('node:assert');
const { decodeGzippedProfile } = require('../common/pprof');
const { encodeSync } = require('node:perf_hooks/pprof');
const {
  serializeTimeProfile,
} = require('internal/perf/pprof/serializer');
const {
  frameFunctionName,
} = require('internal/perf/pprof/normalizer');

/**
 * Builds a profile in the format of the `pprof` binding from nodes, in
 * pre-order, root first.
 * @param {object[]} nodes Nodes with `parent`, `name`, `file`, `scriptId`,
 *   `line`, `column`, `hits` and `cpu` properties, all optional.
 * @param {object} [extra] Properties that override those of the profile.
 * @returns {object} The profile, as the binding's `stop()` returns it.
 */
function makeProfile(nodes, extra = {}) {
  const hasCpuTime = extra.hasCpuTime ?? false;
  /**
   * Returns the values of a property of the nodes.
   * @param {string} key The name of the property.
   * @param {any} defaultValue The value of nodes without the property.
   * @returns {any[]} The value of the property for each node, in order.
   */
  const column = (key, defaultValue) =>
    nodes.map((node) => node[key] ?? defaultValue);
  const hitCount = new Uint32Array(column('hits', 0));
  return {
    startTime: 1000,
    endTime: 1000 + 10000,
    epochOffset: 1_700_000_000_000_000,
    hasCpuTime,
    nonJSThreadsCpuTime: 0,
    totalHitCount: hitCount.reduce((a, b) => a + b, 0),
    restarted: false,
    parent: new Int32Array(column('parent', 0)),
    name: column('name', ''),
    scriptName: column('file', ''),
    scriptId: new Int32Array(column('scriptId', 0)),
    lineNumber: new Int32Array(column('line', 0)),
    columnNumber: new Int32Array(column('column', 0)),
    hitCount,
    cpuTime: hasCpuTime ? new Float64Array(column('cpu', 0)) : undefined,
    ...extra,
  };
}

const root = { parent: -1, name: '(root)' };

/**
 * Serializes a binding profile, encodes it, and decodes the result.
 * @param {object} profile A profile returned by `makeProfile()`.
 * @param {number} [intervalMicros] The sampling interval.
 * @param {boolean} [recompute] Whether to recompute the sampling interval.
 * @returns {object} The result of `decodeGzippedProfile()`.
 */
function decode(profile, intervalMicros = 1000, recompute = false) {
  return decodeGzippedProfile(
    encodeSync(serializeTimeProfile(profile, intervalMicros, recompute)));
}

/**
 * Replaces the frames of decoded samples with their names.
 * @param {object[]} samples Samples returned by `decodeGzippedProfile()`.
 * @returns {Array<{ stack: string[], values: object }>} The samples, with
 *   the names of their frames, leaf first, and their values.
 */
function stacks(samples) {
  return samples.map(({ stack, values }) => ({
    stack: stack.map(({ name }) => name),
    values,
  }));
}

// Without CPU time.
{
  const main = { file: 'file:///app/main.mjs', scriptId: 1 };
  const profile = makeProfile([
    root,
    { name: '(program)', hits: 5 },
    { name: '(idle)', hits: 3 },
    { name: '(garbage collector)', hits: 2 },
    { ...main, name: 'main', line: 1, column: 1 },
    { ...main, parent: 4, name: 'work', line: 5, column: 10, hits: 4 },
    { ...main, parent: 4, name: '', line: 9, column: 3, hits: 1 },
    // A child of `(program)` is removed with it.
    { parent: 1, name: 'removed', hits: 7 },
  ]);
  const { samples, profile: decoded, strings, sampleTypes } = decode(profile);
  assert.deepStrictEqual(sampleTypes, ['sample', 'wall']);
  assert.strictEqual(decoded.period, 1000000);
  assert.strictEqual(decoded.durationNanos, 10000 * 1000);
  // The start time of the profile, since the epoch, rounded to a double.
  assert.strictEqual(BigInt(decoded.timeNanos),
                     BigInt(Number((1_700_000_000_000_000n + 1000n) * 1000n)));
  assert.deepStrictEqual(stacks(samples), [
    {
      stack: ['Garbage Collection', 'Node.js'],
      values: { sample: 2, wall: 2000000 },
    },
    {
      stack: ['work', 'main'],
      values: { sample: 4, wall: 4000000 },
    },
    {
      stack: ['(anonymous:L#9:C#3)', 'main'],
      values: { sample: 1, wall: 1000000 },
    },
  ]);
  // file:// prefixes are removed.
  assert.ok(strings.includes('/app/main.mjs'));
  assert.ok(!strings.some((s) => s.startsWith('file://')));
}

// With CPU time.
{
  const main = { file: '/app/main.js', scriptId: 1 };
  const profile = makeProfile([
    root,
    { name: '(program)', hits: 1 },
    { name: '(idle)', hits: 3, cpu: 500 },
    { ...main, name: 'main', line: 1, column: 1 },
    { ...main, parent: 3, name: 'work', line: 5, column: 10, hits: 4, cpu: 4000 },
  ], { hasCpuTime: true, nonJSThreadsCpuTime: 700 });
  const { samples, sampleTypes } = decode(profile);
  assert.deepStrictEqual(sampleTypes, [
    'sample',
    'wall',
    'cpu',
  ]);
  assert.deepStrictEqual(stacks(samples), [
    // `(idle)` gets no wall time, but keeps its CPU time.
    {
      stack: ['(idle)'],
      values: { sample: 3, wall: 0, cpu: 500 },
    },
    {
      stack: ['work', 'main'],
      values: { sample: 4, wall: 4000000, cpu: 4000 },
    },
    {
      stack: ['Non JS threads activity', 'Node.js'],
      values: { sample: 0, wall: 0, cpu: 700 },
    },
  ]);
}

// Frames with the same position share a location and a function, and
// functions with the same name in the same script share a function.
{
  const main = { file: '/app/main.js', scriptId: 1, column: 1, hits: 1 };
  const profile = makeProfile([
    root,
    { ...main, name: 'a', line: 1 },
    { ...main, parent: 1, name: 'b', line: 2 },
    { ...main, name: 'b', line: 2 },
    { ...main, name: 'b', line: 3 },
  ]);
  const { profile: decoded } = decode(profile);
  assert.strictEqual(decoded.location.length, 3);
  assert.strictEqual(decoded.function.length, 2);
  assert.strictEqual(decoded.sample.length, 4);
}

// The sampling interval is recomputed from the duration and the hit count,
// within [interval, 2 * interval].
{
  const rows = [
    root,
    { name: 'a', file: '/app/main.js', scriptId: 1, line: 1, hits: 8 },
  ];
  // 10000us for 8 hits.
  assert.strictEqual(decode(makeProfile(rows), 1000, true).profile.period,
                     1250000);
  assert.strictEqual(decode(makeProfile(rows), 1000, false).profile.period,
                     1000000);
  assert.strictEqual(decode(makeProfile(rows), 5000, true).profile.period,
                     5000000);
  assert.strictEqual(decode(makeProfile(rows), 500, true).profile.period,
                     1000000);
}

// Without an epoch offset, the start time is estimated from the current time.
{
  const before = BigInt(Date.now()) * 1000000n;
  const profile = makeProfile([root], {
    epochOffset: undefined,
  });
  const { profile: decoded } = decode(profile);
  const after = BigInt(Date.now()) * 1000000n;
  const timeNanos = BigInt(decoded.timeNanos);
  assert.ok(timeNanos >= before - 10000n * 1000n - 1000000n);
  assert.ok(timeNanos <= after);
}

assert.strictEqual(frameFunctionName({ name: 'f' }), 'f');
assert.strictEqual(frameFunctionName({ name: '', line: 3, column: 4 }),
                   '(anonymous:L#3:C#4)');
assert.strictEqual(frameFunctionName({ name: '', line: 3 }),
                   '(anonymous:L#3)');
assert.strictEqual(frameFunctionName({ name: '' }), '(anonymous)');
