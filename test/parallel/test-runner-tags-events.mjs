import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { describe, it, run } from 'node:test';

const fixture = fixtures.path('test-runner', 'tagged.js');

const kEventsWithTags = ['test:enqueue', 'test:dequeue', 'test:start', 'test:pass', 'test:fail', 'test:complete'];

async function collectEvents(opts = {}) {
  const events = [];
  const stream = run({ files: [fixture], ...opts });
  for (const kind of kEventsWithTags) {
    stream.on(kind, (data) => {
      events.push({ kind, name: data.name, tags: data.tags, nesting: data.nesting });
    });
  }
  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
  return events;
}

function indexByName(events) {
  const byName = new Map();
  for (const ev of events) {
    byName.getOrInsert(ev.name, []).push(ev);
  }
  return byName;
}

describe('tag-bearing event payloads', { concurrency: false }, () => {
  it('every event carries a tags array', async () => {
    const events = await collectEvents();
    assert.ok(events.length > 0);
    for (const ev of events) {
      assert.ok(
        Array.isArray(ev.tags),
        `${ev.kind} ${ev.name}: tags should be an array, got ${typeof ev.tags}`,
      );
    }
  });

  it('untagged tests have an empty array, not undefined', async () => {
    const events = await collectEvents();
    const untagged = events.filter((e) => e.name === 'untagged');
    assert.ok(untagged.length > 0, 'expected events for "untagged"');
    for (const ev of untagged) {
      assert.deepStrictEqual(ev.tags, [], `${ev.kind} should have empty tags`);
    }
  });

  it('tag values match the flattened canonical set', async () => {
    const events = await collectEvents();
    const byName = indexByName(events);

    function expectTags(name, expected) {
      const evs = byName.get(name);
      assert.ok(evs && evs.length > 0, `no events for ${name}`);
      for (const ev of evs) {
        assert.deepStrictEqual(ev.tags, expected, `${ev.kind} ${ev.name}`);
      }
    }

    expectTags('db only', ['db']);
    expectTags('db plus integration', ['db', 'integration']);
    expectTags('only flaky', ['flaky']);
    expectTags('db suite', ['db']);
    expectTags('unit slow', ['unit', 'slow']);
  });

  it('all required event kinds carry tags', async () => {
    const events = await collectEvents();
    const seenKinds = new Set(events.map((ev) => ev.kind));
    for (const required of ['test:enqueue', 'test:start', 'test:pass', 'test:complete']) {
      assert.ok(seenKinds.has(required), `expected ${required}`);
    }
    for (const ev of events) {
      assert.ok(
        kEventsWithTags.includes(ev.kind),
        `unexpected event kind ${ev.kind}`,
      );
    }
  });

  it('test:pass fires only for selected tagged tests when filtered', async () => {
    // isolation='none' so the parent applies the filter directly. Under
    // 'process', the FileTest wrapper (which has no tags) would itself be
    // filtered out by the include filter - same wart as --test-name-pattern.
    const stream = run({ files: [fixture], testTagFilters: ['db'], isolation: 'none' });
    stream.on('test:fail', common.mustNotCall());
    // 3 db-tagged tests pass + the db suite itself.
    stream.on('test:pass', common.mustCall(4));
    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });
});
