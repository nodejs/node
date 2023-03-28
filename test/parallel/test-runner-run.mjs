import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { join } from 'node:path';
import { describe, it, run } from 'node:test';
import { dot, spec, tap } from 'node:test/reporters';
import assert from 'node:assert';

const testFixtures = fixtures.path('test-runner');

describe('require(\'node:test\').run', { concurrency: true }, () => {

  it('should run with no tests', async () => {
    const stream = run({ files: [] });
    stream.on('test:fail', common.mustNotCall());
    stream.on('test:pass', common.mustNotCall());
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });

  it('should fail with non existing file', async () => {
    const stream = run({ files: ['a-random-file-that-does-not-exist.js'] });
    stream.on('test:fail', common.mustCall(1));
    stream.on('test:pass', common.mustNotCall());
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });

  it('should succeed with a file', async () => {
    const stream = run({ files: [join(testFixtures, 'test/random.cjs')] });
    stream.on('test:fail', common.mustNotCall());
    stream.on('test:pass', common.mustCall(1));
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });

  it('should run same file twice', async () => {
    const stream = run({ files: [join(testFixtures, 'test/random.cjs'), join(testFixtures, 'test/random.cjs')] });
    stream.on('test:fail', common.mustNotCall());
    stream.on('test:pass', common.mustCall(2));
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });

  it('should run a failed test', async () => {
    const stream = run({ files: [testFixtures] });
    stream.on('test:fail', common.mustCall(1));
    stream.on('test:pass', common.mustNotCall());
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });

  it('should support timeout', async () => {
    const stream = run({ timeout: 50, files: [
      fixtures.path('test-runner', 'never_ending_sync.js'),
      fixtures.path('test-runner', 'never_ending_async.js'),
    ] });
    stream.on('test:fail', common.mustCall(2));
    stream.on('test:pass', common.mustNotCall());
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });

  it('should validate files', async () => {
    [Symbol(), {}, () => {}, 0, 1, 0n, 1n, '', '1', Promise.resolve([]), true, false]
      .forEach((files) => assert.throws(() => run({ files }), {
        code: 'ERR_INVALID_ARG_TYPE'
      }));
  });

  it('should be piped with dot', async () => {
    const result = await run({ files: [join(testFixtures, 'test/random.cjs')] }).compose(dot).toArray();
    assert.deepStrictEqual(result, [
      '.',
      '\n',
    ]);
  });

  it('should be piped with spec', async () => {
    const specReporter = new spec();
    const result = await run({ files: [join(testFixtures, 'test/random.cjs')] }).compose(specReporter).toArray();
    const stringResults = result.map((bfr) => bfr.toString());
    assert.match(stringResults[0], /this should pass/);
    assert.match(stringResults[1], /tests 1/);
    assert.match(stringResults[1], /pass 1/);
  });

  it('should be piped with tap', async () => {
    const result = await run({ files: [join(testFixtures, 'test/random.cjs')] }).compose(tap).toArray();
    assert.strictEqual(result.length, 13);
    assert.strictEqual(result[0], 'TAP version 13\n');
    assert.strictEqual(result[1], '# Subtest: this should pass\n');
    assert.strictEqual(result[2], 'ok 1 - this should pass\n');
    assert.match(result[3], /duration_ms: \d+\.?\d*/);
    assert.strictEqual(result[4], '1..1\n');
    assert.strictEqual(result[5], '# tests 1\n');
    assert.strictEqual(result[6], '# suites 0\n');
    assert.strictEqual(result[7], '# pass 1\n');
    assert.strictEqual(result[8], '# fail 0\n');
    assert.strictEqual(result[9], '# cancelled 0\n');
    assert.strictEqual(result[10], '# skipped 0\n');
    assert.strictEqual(result[11], '# todo 0\n');
    assert.match(result[12], /# duration_ms \d+\.?\d*/);
  });
});
