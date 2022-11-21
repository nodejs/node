import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { join } from 'node:path';
import { describe, it, run } from 'node:test';
import assert from 'node:assert';

const testFixtures = fixtures.path('test-runner');

describe('require(\'node:test\').run', { concurrency: true }, () => {

  it('should run with no tests', async () => {
    const stream = run({ files: [] });
    stream.setEncoding('utf8');
    stream.on('test:fail', common.mustNotCall());
    stream.on('test:pass', common.mustNotCall());
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream); // TODO(MoLow): assert.snapshot
  });

  it('should fail with non existing file', async () => {
    const stream = run({ files: ['a-random-file-that-does-not-exist.js'] });
    stream.on('test:fail', common.mustCall(1));
    stream.on('test:pass', common.mustNotCall());
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream); // TODO(MoLow): assert.snapshot
  });

  it('should succeed with a file', async () => {
    const stream = run({ files: [join(testFixtures, 'test/random.cjs')] });
    stream.on('test:fail', common.mustNotCall());
    stream.on('test:pass', common.mustCall(2));
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream); // TODO(MoLow): assert.snapshot
  });

  it('should run same file twice', async () => {
    const stream = run({ files: [join(testFixtures, 'test/random.cjs'), join(testFixtures, 'test/random.cjs')] });
    stream.on('test:fail', common.mustNotCall());
    stream.on('test:pass', common.mustCall(4));
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream); // TODO(MoLow): assert.snapshot
  });

  it('should run a failed test', async () => {
    const stream = run({ files: [testFixtures] });
    stream.on('test:fail', common.mustCall(1));
    stream.on('test:pass', common.mustNotCall());
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream); // TODO(MoLow): assert.snapshot
  });

  it('should support timeout', async () => {
    const stream = run({ timeout: 50, files: [
      fixtures.path('test-runner', 'never_ending_sync.js'),
      fixtures.path('test-runner', 'never_ending_async.js'),
    ] });
    stream.on('test:fail', common.mustCall(2));
    stream.on('test:pass', common.mustNotCall());
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream); // TODO(MoLow): assert.snapshot
  });

  it('should validate files', async () => {
    [Symbol(), {}, () => {}, 0, 1, 0n, 1n, '', '1', Promise.resolve([]), true, false]
      .forEach((files) => assert.throws(() => run({ files }), {
        code: 'ERR_INVALID_ARG_TYPE'
      }));
  });
});
