'use strict';

// Checks the events that fs/promises functions and FileHandle methods
// publish.

const common = require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const fs = require('node:fs');
const fsp = require('node:fs/promises');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const file = tmpdir.resolve('file.txt');
const missing = tmpdir.resolve('missing.txt');
fs.writeFileSync(file, 'hello');

function record(name) {
  const events = [];
  const channel = dc.tracingChannel(`fs.${name}`);
  const handlers = {};
  for (const type of ['start', 'end', 'asyncStart', 'asyncEnd', 'error']) {
    handlers[type] = (context) => events.push({ type, context });
  }
  channel.subscribe(handlers);
  return {
    events,
    types: () => events.map((e) => e.type),
    stop: () => channel.unsubscribe(handlers),
  };
}

function assertSameContext(events) {
  for (const { context } of events) {
    assert.strictEqual(context, events[0].context);
  }
}

(async () => {
  {
    const stat = record('stat');
    const promise = fsp.stat(file);
    const stats = await promise;
    stat.stop();
    assert.deepStrictEqual(stat.types(), ['start', 'end', 'asyncStart', 'asyncEnd']);
    assertSameContext(stat.events);
    const { context } = stat.events[0];
    assert.strictEqual(context.api, 'promise');
    assert.strictEqual(context.path, file);
    assert.strictEqual(context.result, stats);
    assert.ok(stats instanceof fs.Stats);
  }

  {
    const stat = record('stat');
    const err = await fsp.stat(missing).catch((err) => err);
    stat.stop();
    assert.strictEqual(err.code, 'ENOENT');
    assert.deepStrictEqual(stat.types(),
                           ['start', 'end', 'error', 'asyncStart', 'asyncEnd']);
    assertSameContext(stat.events);
    assert.strictEqual(stat.events[0].context.error, err);
  }

  {
    // Invalid arguments are reported like other errors.
    const stat = record('stat');
    await assert.rejects(fsp.stat(123), { code: 'ERR_INVALID_ARG_TYPE' });
    stat.stop();
    assert.deepStrictEqual(stat.types(),
                           ['start', 'end', 'error', 'asyncStart', 'asyncEnd']);
  }

  {
    const readFile = record('readFile');
    const data = await fsp.readFile(file, 'utf8');
    readFile.stop();
    assert.deepStrictEqual(readFile.types(), ['start', 'end', 'asyncStart', 'asyncEnd']);
    assert.strictEqual(readFile.events[0].context.path, file);
    assert.strictEqual(readFile.events[0].context.result, data);
  }

  {
    const writeFile = record('writeFile');
    const data = Buffer.from('written');
    await fsp.writeFile(file, data);
    writeFile.stop();
    assert.deepStrictEqual(writeFile.types(), ['start', 'end', 'asyncStart', 'asyncEnd']);
    assert.strictEqual(writeFile.events[0].context.path, file);
    assert.strictEqual(writeFile.events[0].context.data, data);
  }

  {
    // FileHandle methods publish with the descriptor of the handle.
    const open = record('open');
    const read = record('read');
    const fstat = record('fstat');
    const close = record('close');

    const handle = await fsp.open(file, 'r');
    const { fd } = handle;
    assert.deepStrictEqual(open.types(), ['start', 'end', 'asyncStart', 'asyncEnd']);
    assert.strictEqual(open.events[0].context.path, file);
    assert.strictEqual(open.events[0].context.result, handle);

    const buffer = Buffer.alloc(7);
    const result = await handle.read(buffer, 0, 7, 0);
    assert.deepStrictEqual(read.types(), ['start', 'end', 'asyncStart', 'asyncEnd']);
    assert.strictEqual(read.events[0].context.api, 'promise');
    assert.strictEqual(read.events[0].context.fd, fd);
    assert.deepStrictEqual(read.events[0].context.args, [buffer, 0, 7, 0]);
    assert.strictEqual(read.events[0].context.result, result);
    assert.strictEqual(result.bytesRead, 7);

    const stats = await handle.stat();
    assert.deepStrictEqual(fstat.types(), ['start', 'end', 'asyncStart', 'asyncEnd']);
    assert.strictEqual(fstat.events[0].context.fd, fd);
    assert.strictEqual(fstat.events[0].context.result, stats);

    await handle.close();
    assert.deepStrictEqual(close.types(), ['start', 'end', 'asyncStart', 'asyncEnd']);
    assert.strictEqual(close.events[0].context.api, 'promise');
    assert.strictEqual(close.events[0].context.fd, fd);

    open.stop();
    read.stop();
    fstat.stop();
    close.stop();
  }

  {
    const closedir = record('closedir');
    const dir = fs.opendirSync(tmpdir.path);
    await dir.close();
    closedir.stop();
    assert.deepStrictEqual(closedir.types(), ['start', 'end', 'asyncStart', 'asyncEnd']);
    assert.strictEqual(closedir.events[0].context.api, 'promise');
    assert.deepStrictEqual(closedir.events[0].context.args, []);
  }
})().then(common.mustCall());
