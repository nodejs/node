'use strict';

require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { join } = require('node:path');
const { describe, it } = require('node:test');
const fixtures = require('../common/fixtures');

const testFixtures = fixtures.path('test-runner');
const internalOrderFixture = fixtures.path('test-runner', 'randomize', 'internal-order.cjs');
const rerunStateFile = fixtures.path('test-runner', 'rerun-state.json');
const kShardFiles = ['a.cjs', 'b.cjs', 'c.cjs', 'd.cjs', 'e.cjs', 'f.cjs', 'g.cjs', 'h.cjs', 'i.cjs', 'j.cjs'];
const kInternalTests = ['a', 'b', 'c', 'd', 'e'];

function getShardOrder(stdout) {
  return Array.from(stdout.matchAll(/ok \d+ - ([a-j]\.cjs) this should pass/g), ({ 1: name }) => name);
}

function getInternalExecutionOrder(stdout) {
  const match = stdout.match(/EXECUTION_ORDER:([a-e](?:,[a-e])*)/);
  assert(match, `Missing EXECUTION_ORDER marker in output: ${stdout}`);
  return match[1].split(',');
}

describe('test runner randomize flags via command line', () => {
  it('should be deterministic with --test-random-seed', () => {
    const args = [
      '--test',
      '--test-reporter=tap',
      '--test-concurrency=1',
      '--test-randomize',
      '--test-random-seed=12345',
      join(testFixtures, 'shards/*.cjs'),
    ];
    const first = spawnSync(process.execPath, args);
    const second = spawnSync(process.execPath, args);

    assert.strictEqual(first.stderr.toString(), '');
    assert.strictEqual(second.stderr.toString(), '');
    assert.strictEqual(first.status, 0);
    assert.strictEqual(second.status, 0);

    const firstOrder = getShardOrder(first.stdout.toString());
    const secondOrder = getShardOrder(second.stdout.toString());
    assert.deepStrictEqual(firstOrder, secondOrder);
    assert.deepStrictEqual([...firstOrder].sort(), kShardFiles);
  });

  it('should use different orders for different seeds', () => {
    const first = spawnSync(process.execPath, [
      '--test',
      '--test-reporter=tap',
      '--test-concurrency=1',
      '--test-randomize',
      '--test-random-seed=11111',
      join(testFixtures, 'shards/*.cjs'),
    ]);
    const second = spawnSync(process.execPath, [
      '--test',
      '--test-reporter=tap',
      '--test-concurrency=1',
      '--test-randomize',
      '--test-random-seed=22222',
      join(testFixtures, 'shards/*.cjs'),
    ]);

    assert.strictEqual(first.stderr.toString(), '');
    assert.strictEqual(second.stderr.toString(), '');
    assert.strictEqual(first.status, 0);
    assert.strictEqual(second.status, 0);

    const firstOrder = getShardOrder(first.stdout.toString());
    const secondOrder = getShardOrder(second.stdout.toString());
    assert.notDeepStrictEqual(firstOrder, secondOrder);
    assert.deepStrictEqual([...firstOrder].sort(), kShardFiles);
    assert.deepStrictEqual([...secondOrder].sort(), kShardFiles);
  });

  it('should randomize deterministically with --test-random-seed alone', () => {
    const args = [
      '--test',
      '--test-reporter=tap',
      '--test-concurrency=1',
      '--test-random-seed=24680',
      join(testFixtures, 'shards/*.cjs'),
    ];
    const first = spawnSync(process.execPath, args);
    const second = spawnSync(process.execPath, args);

    assert.strictEqual(first.stderr.toString(), '');
    assert.strictEqual(second.stderr.toString(), '');
    assert.strictEqual(first.status, 0);
    assert.strictEqual(second.status, 0);

    const firstOrder = getShardOrder(first.stdout.toString());
    const secondOrder = getShardOrder(second.stdout.toString());
    assert.deepStrictEqual(firstOrder, secondOrder);
    assert.deepStrictEqual([...firstOrder].sort(), kShardFiles);
    assert.match(first.stdout.toString(), /# Randomized test order seed: 24680/);
  });

  it('should print the randomization seed when --test-randomize is used', () => {
    const child = spawnSync(process.execPath, [
      '--test',
      '--test-reporter=tap',
      '--test-concurrency=1',
      '--test-randomize',
      join(testFixtures, 'shards/*.cjs'),
    ]);

    assert.strictEqual(child.stderr.toString(), '');
    assert.strictEqual(child.status, 0);
    assert.match(child.stdout.toString(), /# Randomized test order seed: \d+/);
  });

  it('should randomize internal test order deterministically with --test-random-seed', () => {
    const args = [
      '--test',
      '--test-reporter=tap',
      '--test-random-seed=12345',
      internalOrderFixture,
    ];
    const first = spawnSync(process.execPath, args);
    const second = spawnSync(process.execPath, args);

    assert.strictEqual(first.stderr.toString(), '');
    assert.strictEqual(second.stderr.toString(), '');
    assert.strictEqual(first.status, 0);
    assert.strictEqual(second.status, 0);

    const firstOrder = getInternalExecutionOrder(first.stdout.toString());
    const secondOrder = getInternalExecutionOrder(second.stdout.toString());
    assert.deepStrictEqual(firstOrder, secondOrder);
    assert.deepStrictEqual([...firstOrder].sort(), kInternalTests);
  });

  it('should randomize internal test order differently across seeds', () => {
    const orders = [];
    for (const seed of [11111, 22222, 33333, 44444]) {
      const child = spawnSync(process.execPath, [
        '--test',
        '--test-reporter=tap',
        `--test-random-seed=${seed}`,
        internalOrderFixture,
      ]);
      assert.strictEqual(child.stderr.toString(), '');
      assert.strictEqual(child.status, 0);

      const order = getInternalExecutionOrder(child.stdout.toString());
      assert.deepStrictEqual([...order].sort(), kInternalTests);
      orders.push(order.join(','));
    }

    assert.notStrictEqual(new Set(orders).size, 1);
  });

  it('should reject --test-randomize with --watch', () => {
    const child = spawnSync(process.execPath, [
      '--test',
      '--watch',
      '--test-randomize',
      join(testFixtures, 'shards/*.cjs'),
    ]);

    assert.strictEqual(child.stdout.toString(), '');
    assert.match(child.stderr.toString(), /The property 'options\.randomize' is not supported with watch mode\./);
    assert.strictEqual(child.status, 1);
    assert.strictEqual(child.signal, null);
  });

  it('should reject --test-random-seed with --watch', () => {
    const child = spawnSync(process.execPath, [
      '--test',
      '--watch',
      '--test-random-seed=12345',
      join(testFixtures, 'shards/*.cjs'),
    ]);

    assert.strictEqual(child.stdout.toString(), '');
    assert.match(child.stderr.toString(), /The property 'options\.randomSeed' is not supported with watch mode\./);
    assert.strictEqual(child.status, 1);
    assert.strictEqual(child.signal, null);
  });

  it('should reject --test-randomize with --test-rerun-failures', () => {
    const child = spawnSync(process.execPath, [
      '--test',
      '--test-randomize',
      '--test-rerun-failures',
      rerunStateFile,
      join(testFixtures, 'shards/*.cjs'),
    ]);

    assert.strictEqual(child.stdout.toString(), '');
    assert.match(child.stderr.toString(), /The property 'options\.randomize' is not supported with rerun failures mode\./);
    assert.strictEqual(child.status, 1);
    assert.strictEqual(child.signal, null);
  });

  it('should reject --test-random-seed with --test-rerun-failures', () => {
    const child = spawnSync(process.execPath, [
      '--test',
      '--test-random-seed=12345',
      '--test-rerun-failures',
      rerunStateFile,
      join(testFixtures, 'shards/*.cjs'),
    ]);

    assert.strictEqual(child.stdout.toString(), '');
    assert.match(child.stderr.toString(), /The property 'options\.randomSeed' is not supported with rerun failures mode\./);
    assert.strictEqual(child.status, 1);
    assert.strictEqual(child.signal, null);
  });

  it('should reject out of range --test-random-seed values', () => {
    const child = spawnSync(process.execPath, [
      '--test',
      '--test-random-seed=4294967296',
      join(testFixtures, 'shards/*.cjs'),
    ]);

    assert.strictEqual(child.stdout.toString(), '');
    assert.match(child.stderr.toString(), /The value of "--test-random-seed" is out of range\. It must be >= 0 && <= 4294967295\. Received 4294967296/);
    assert.strictEqual(child.status, 1);
    assert.strictEqual(child.signal, null);
  });
});
