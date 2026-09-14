'use strict';

require('../common');

const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const dc = require('node:diagnostics_channel');
const { it } = require('node:test');
const zlib = require('node:zlib');

it('publishes timestamps without a redundant type', async () => {
  const events = [];
  const handler = (event) => events.push(event);
  dc.subscribe('threadpool.work.zlib', handler);

  await new Promise((resolve, reject) => {
    zlib.gzip('data', (err) => {
      if (err) reject(err);
      else resolve();
    });
  });

  dc.unsubscribe('threadpool.work.zlib', handler);
  const [event] = events;
  assert.deepStrictEqual(Object.keys(event),
                         ['enqueued', 'started', 'ended']);
  assert.ok(event.enqueued <= event.started);
  assert.ok(event.started <= event.ended);
  assert.ok(event.ended <= performance.now());
});

it('does not relink channels while constructing work', () => {
  const child = spawnSync(process.execPath, ['-e', `
    const assert = require('node:assert');
    const dc = require('node:diagnostics_channel');
    const channel = dc.channel('threadpool.work.zlib');
    const index = channel._index;
    let writes = 0;
    Object.defineProperty(channel, '_index', {
      __proto__: null,
      configurable: true,
      get: () => index,
      set: () => {
        writes++;
        throw new Error('unexpected channel relink');
      },
    });
    require('node:zlib').gzip('data', (err) => {
      if (err) throw err;
      assert.strictEqual(writes, 0);
    });
  `], { encoding: 'utf8' });
  assert.strictEqual(child.status, 0, child.stderr);
});
