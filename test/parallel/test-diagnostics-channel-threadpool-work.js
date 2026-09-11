'use strict';

const common = require('../common');

const assert = require('node:assert');
const crypto = common.hasCrypto ? require('node:crypto') : null;
const dc = require('node:diagnostics_channel');
const fs = require('node:fs');
const { afterEach, beforeEach, describe, it } = require('node:test');
const { Worker } = require('node:worker_threads');
const zlib = require('node:zlib');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const CHANNEL = 'threadpool.work';

function assertTimestamps(event, now = performance.now()) {
  assert.ok(event.enqueued <= event.started);
  assert.ok(event.started <= event.ended);
  assert.ok(event.ended <= now);
}

describe('threadpool.work diagnostics channel', () => {
  let events;
  const handler = (message) => events.push(message);

  beforeEach(() => {
    events = [];
    dc.subscribe(CHANNEL, handler);
  });

  afterEach(() => dc.unsubscribe(CHANNEL, handler));

  it('publishes for zlib work', async () => {
    await new Promise((resolve, reject) => {
      zlib.gzip(Buffer.alloc(64 * 1024, 'x'), (err) => {
        if (err) reject(err);
        else resolve();
      });
    });

    assert.ok(events.some((event) => event.type === 'zlib'));
  });

  it('publishes for crypto work', { skip: !common.hasCrypto }, async () => {
    await new Promise((resolve, reject) => {
      crypto.pbkdf2('secret', 'salt', 100, 32, 'sha256', (err) => {
        if (err) reject(err);
        else resolve();
      });
    });

    const event = events.find((event) => event.type === 'crypto');
    assert.ok(event);
    assert.deepStrictEqual(Object.keys(event),
                           ['type', 'enqueued', 'started', 'ended']);
    assertTimestamps(event);
  });

  it('publishes for fs work', async () => {
    await Promise.all([
      fs.promises.readFile(__filename),
      fs.promises.writeFile(tmpdir.resolve('threadpool-work'), 'data'),
    ]);

    assert.ok(events.some((event) => event.type === 'fs.readfile'));
    assert.ok(events.some((event) => event.type === 'fs.writefile'));
  });

  it('publishes in worker threads', async () => {
    const result = await new Promise((resolve, reject) => {
      const worker = new Worker(`
        const dc = require('node:diagnostics_channel');
        const { parentPort } = require('node:worker_threads');
        const zlib = require('node:zlib');
        const events = [];
        const handler = (event) => events.push(event);
        dc.subscribe('threadpool.work', handler);
        zlib.gzip('data', (err) => {
          if (err) throw err;
          dc.unsubscribe('threadpool.work', handler);
          parentPort.postMessage({
            event: events.find((event) => event.type === 'zlib'),
            now: performance.now(),
          });
        });
      `, { eval: true });
      worker.once('message', resolve);
      worker.once('error', reject);
    });

    assert.ok(result.event);
    assertTimestamps(result.event, result.now);
  });

  it('stays monotonic across repeated submissions of one instance', async () => {
    await new Promise((resolve, reject) => {
      const gzip = zlib.createGzip();
      gzip.once('error', reject);
      gzip.on('end', resolve);
      gzip.resume();
      for (let i = 0; i < 20; i++) gzip.write(Buffer.alloc(8192, i));
      gzip.end();
    });

    const zlibEvents = events.filter((event) => event.type === 'zlib');
    assert.ok(zlibEvents.length > 1);
    for (let i = 0; i < zlibEvents.length; i++) {
      assertTimestamps(zlibEvents[i]);
      if (i > 0) {
        assert.ok(zlibEvents[i - 1].ended <= zlibEvents[i].enqueued);
      }
    }
  });

  it('stops publishing after unsubscribe', async () => {
    const pending = new Promise((resolve, reject) => {
      zlib.gzip(Buffer.alloc(64 * 1024, 'x'), (err) => {
        if (err) reject(err);
        else resolve();
      });
    });

    dc.unsubscribe(CHANNEL, handler);
    await pending;
    assert.ok(!events.some((event) => event.type === 'zlib'));
  });

});
