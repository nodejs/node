import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { describe, it, run } from 'node:test';
import assert from 'node:assert';

const aPath = fixtures.path('test-runner', 'entry-file', 'a.test.mjs');
const bPath = fixtures.path('test-runner', 'entry-file', 'b.test.mjs');
const helperPath = fixtures.path('test-runner', 'entry-file', 'helper.mjs');

async function collectEvents(options) {
  const events = [];
  const stream = run({ files: [aPath, bPath], ...options });
  stream.on('test:fail', () => {});
  for await (const event of stream) {
    events.push(event);
  }
  return events;
}

describe('entryFile attribution in reporter events', { concurrency: false }, () => {
  it('stamps entryFile on events forwarded from child processes', async () => {
    const events = await collectEvents({ isolation: 'process' });
    const checked = { __proto__: null, A: 0, B: 0 };

    for (const { type, data } of events) {
      if (data?.name === 'restore A' || data?.name === 'restore B') {
        const target = data.name === 'restore A' ? 'A' : 'B';
        const expectedEntry = target === 'A' ? aPath : bPath;
        assert.strictEqual(data.file, helperPath,
                           `${type} file should be the definition site`);
        assert.strictEqual(data.entryFile, expectedEntry,
                           `${type} entryFile should be the entry file`);
        checked[target]++;
      }
    }

    // Each subtest emits at least enqueue/dequeue/start/pass/complete.
    assert.ok(checked.A >= 4, `expected events for restore A, got ${checked.A}`);
    assert.ok(checked.B >= 4, `expected events for restore B, got ${checked.B}`);
  });

  it('stamps entryFile on top-level tests forwarded from child processes', async () => {
    const events = await collectEvents({ isolation: 'process' });
    const pass = events.filter(({ type }) => type === 'test:pass');
    const backupA = pass.find(({ data }) => data.name === 'backup A');
    const backupB = pass.find(({ data }) => data.name === 'backup B');
    assert.strictEqual(backupA.data.entryFile, aPath);
    assert.strictEqual(backupB.data.entryFile, bPath);
  });

  it('does not stamp entryFile with isolation none', async () => {
    const events = await collectEvents({ isolation: 'none' });
    for (const { data } of events) {
      if (data?.name === 'restore A' || data?.name === 'restore B') {
        assert.strictEqual(data.entryFile, undefined);
      }
    }
  });
});
