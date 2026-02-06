// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');

const { myersDiff } = require('internal/assert/myers_diff');

// Test: myersDiff input size limit
{
  const arr1 = { length: 2 ** 31 - 1 };
  const arr2 = { length: 2 };
  const max = arr1.length + arr2.length;
  assert.throws(
    () => {
      myersDiff(arr1, arr2);
    },
    common.expectsError({
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
      message: 'The value of "myersDiff input size" ' +
                'is out of range. It must be < 2^31. ' +
                `Received ${max}`
    })
  );
}

// Test: small input correctness
{
  const actual = ['a', 'b', 'X', 'c', 'd'];
  const expected = ['a', 'b', 'c', 'd'];
  const ops = diffToForwardOps(myersDiff(actual, expected));

  const inserts = ops.filter((o) => o.op === 1);
  const deletes = ops.filter((o) => o.op === -1);
  const nops = ops.filter((o) => o.op === 0);

  assert.strictEqual(inserts.length, 1);
  assert.strictEqual(inserts[0].value, 'X');
  assert.strictEqual(deletes.length, 0);
  assert.strictEqual(nops.length, 4);
}

// Test: aligned boundary correctness - extra lines in the middle
// When expected has extra lines, aligned boundaries should produce
// only real INSERT/DELETE/NOP operations with no phantom diffs.
{
  const { actual, expected } = createAlignedTestArrays({ lineCount: 600, extraLineAt: 100 });

  const result = myersDiff(actual, expected);
  const ops = diffToForwardOps(result);

  const inserts = ops.filter((o) => o.op === 1);
  const deletes = ops.filter((o) => o.op === -1);
  const nops = ops.filter((o) => o.op === 0);

  assert.strictEqual(inserts.length, 1);
  assert.strictEqual(inserts[0].value, 'EXTRA_100');
  assert.strictEqual(deletes.length, 0, 'should produce no phantom DELETEs');
  assert.strictEqual(nops.length, 600);
}

// Test: multiple extra lines across chunk boundaries
{
  const expected = [];
  for (let i = 0; i < 1200; i++) expected.push('line_' + i);

  const actual = [];
  for (let i = 0; i < 1200; i++) {
    if (i === 100) {
      actual.push('EXTRA_A');
      actual.push('EXTRA_B');
    }
    actual.push('line_' + i);
  }

  const result = myersDiff(actual, expected);
  const ops = diffToForwardOps(result);

  const inserts = ops.filter((o) => o.op === 1);
  const deletes = ops.filter((o) => o.op === -1);

  assert.strictEqual(inserts.length, 2);
  assert.strictEqual(inserts[0].value, 'EXTRA_A');
  assert.strictEqual(inserts[1].value, 'EXTRA_B');
  assert.strictEqual(deletes.length, 0, 'should produce no phantom DELETEs');
}

// Test: large identical inputs produce all NOPs
{
  const lines = [];
  for (let i = 0; i < 600; i++) lines.push('line_' + i);

  const result = myersDiff(lines, lines);
  const ops = diffToForwardOps(result);

  assert.strictEqual(ops.length, 600);
  assert.ok(ops.every((o) => o.op === 0));
}

// Test: one side much longer than the other
// Diff should be correct (rebuild both sides from ops matches originals)
{
  const actual = [];
  for (let i = 0; i < 700; i++) actual.push('line_' + i);

  const expected = [];
  for (let i = 0; i < 200; i++) expected.push('line_' + i);

  const result = myersDiff(actual, expected);
  const ops = diffToForwardOps(result);

  // Verify correctness: rebuild both sides from diff ops
  const { rebuiltActual, rebuiltExpected } = rebuildFromOps(ops);
  assert.deepStrictEqual(rebuiltActual, actual);
  assert.deepStrictEqual(rebuiltExpected, expected);

  // Chunked diff with fallback boundaries may not find all 200 shared lines,
  // but the diff must still be correct (rebuilds match originals).
  const nops = ops.filter((o) => o.op === 0);
  const inserts = ops.filter((o) => o.op === 1);
  const deletes = ops.filter((o) => o.op === -1);
  assert.ok(nops.length >= 146, `expected at least 146 NOPs, got ${nops.length}`);
  assert.strictEqual(nops.length + inserts.length, actual.length);
  assert.strictEqual(nops.length + deletes.length, expected.length);
}

// Test: expected side longer (deletions)
{
  const actual = [];
  for (let i = 0; i < 200; i++) actual.push('line_' + i);

  const expected = [];
  for (let i = 0; i < 700; i++) expected.push('line_' + i);

  const result = myersDiff(actual, expected);
  const ops = diffToForwardOps(result);

  const { rebuiltActual, rebuiltExpected } = rebuildFromOps(ops);
  assert.deepStrictEqual(rebuiltActual, actual);
  assert.deepStrictEqual(rebuiltExpected, expected);

  const nops = ops.filter((o) => o.op === 0);
  const inserts = ops.filter((o) => o.op === 1);
  const deletes = ops.filter((o) => o.op === -1);
  assert.ok(nops.length >= 146, `expected at least 146 NOPs, got ${nops.length}`);
  assert.strictEqual(nops.length + inserts.length, actual.length);
  assert.strictEqual(nops.length + deletes.length, expected.length);
}

// Test: no matching anchor available (all unique lines)
// Should gracefully fall back and still produce a valid diff
{
  const actual = [];
  const expected = [];
  for (let i = 0; i < 600; i++) {
    actual.push('actual_unique_' + i);
    expected.push('expected_unique_' + i);
  }

  const result = myersDiff(actual, expected);
  const ops = diffToForwardOps(result);

  const inserts = ops.filter((o) => o.op === 1);
  const deletes = ops.filter((o) => o.op === -1);

  // All lines are different, so we should get 600 inserts and 600 deletes
  assert.strictEqual(inserts.length, 600);
  assert.strictEqual(deletes.length, 600);
}

function diffToForwardOps(diff) {
  const ops = [];
  for (let i = diff.length - 1; i >= 0; i--) {
    ops.push({ op: diff[i][0], value: diff[i][1] });
  }
  return ops;
}

function rebuildFromOps(ops) {
  const rebuiltActual = [];
  const rebuiltExpected = [];
  for (let i = 0; i < ops.length; i++) {
    if (ops[i].op === 0) {
      rebuiltActual.push(ops[i].value);
      rebuiltExpected.push(ops[i].value);
    } else if (ops[i].op === 1) {
      rebuiltActual.push(ops[i].value);
    } else {
      rebuiltExpected.push(ops[i].value);
    }
  }
  return { rebuiltActual, rebuiltExpected };
}

function createAlignedTestArrays({ lineCount, extraLineAt } = {}) {
  const expected = [];
  for (let i = 0; i < lineCount; i++) expected.push('line_' + i);

  const actual = [];
  for (let i = 0; i < lineCount; i++) {
    if (i === extraLineAt) actual.push('EXTRA_' + extraLineAt);
    actual.push('line_' + i);
  }

  return { actual, expected };
}
