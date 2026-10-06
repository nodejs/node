'use strict';

// Checks the events that synchronous fs functions publish.

const common = require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const fs = require('node:fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// Captured before any subscriber exists.
const { statSync } = fs;

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

const file = tmpdir.resolve('file.txt');
const missing = tmpdir.resolve('missing.txt');
fs.writeFileSync(file, 'hello');

{
  const stat = record('stat');
  const stats = statSync(file);
  assert.deepStrictEqual(stat.types(), ['start', 'end']);
  assertSameContext(stat.events);
  const { context } = stat.events[0];
  assert.strictEqual(context.api, 'sync');
  assert.strictEqual(context.path, file);
  assert.strictEqual(context.result, stats);
  assert.ok(context.result instanceof fs.Stats);
  stat.stop();
}

{
  // The error event comes before end.
  const stat = record('stat');
  assert.throws(() => fs.statSync(missing), { code: 'ENOENT' });
  assert.deepStrictEqual(stat.types(), ['start', 'error', 'end']);
  assertSameContext(stat.events);
  assert.strictEqual(stat.events[0].context.error.code, 'ENOENT');
  assert.strictEqual(stat.events[0].context.path, missing);
  stat.stop();
}

{
  // Invalid arguments are reported like other errors.
  const stat = record('stat');
  assert.throws(() => fs.statSync(123), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.deepStrictEqual(stat.types(), ['start', 'error', 'end']);
  assert.strictEqual(stat.events[0].context.error.code, 'ERR_INVALID_ARG_TYPE');
  stat.stop();
}

{
  const open = record('open');
  const read = record('read');
  const close = record('close');

  const fd = fs.openSync(file, 'r');
  assert.deepStrictEqual(open.types(), ['start', 'end']);
  assert.strictEqual(open.events[0].context.path, file);
  assert.deepStrictEqual(open.events[0].context.args, [file, 'r']);
  assert.strictEqual(open.events[0].context.result, fd);

  const buffer = Buffer.alloc(5);
  const bytesRead = fs.readSync(fd, buffer, 0, 5, 0);
  assert.deepStrictEqual(read.types(), ['start', 'end']);
  const readContext = read.events[0].context;
  assert.strictEqual(readContext.fd, fd);
  assert.strictEqual(readContext.args[1], buffer);
  assert.deepStrictEqual(readContext.args.slice(2), [0, 5, 0]);
  assert.strictEqual(readContext.result, bytesRead);

  fs.closeSync(fd);
  assert.deepStrictEqual(close.types(), ['start', 'end']);
  assert.strictEqual(close.events[0].context.fd, fd);

  open.stop();
  read.stop();
  close.stop();
}

{
  const writeFile = record('writeFile');
  const readFile = record('readFile');
  const data = 'new contents';

  fs.writeFileSync(file, data);
  assert.deepStrictEqual(writeFile.types(), ['start', 'end']);
  assert.strictEqual(writeFile.events[0].context.path, file);
  assert.strictEqual(writeFile.events[0].context.data, data);

  // The utf8 fast path and the generic path both publish.
  const text = fs.readFileSync(file, 'utf8');
  const buffer = fs.readFileSync(file);
  assert.deepStrictEqual(readFile.types(), ['start', 'end', 'start', 'end']);
  assert.strictEqual(readFile.events[1].context.result, text);
  assert.strictEqual(readFile.events[3].context.result, buffer);

  writeFile.stop();
  readFile.stop();
}

{
  const exists = record('exists');
  assert.strictEqual(fs.existsSync(file), true);
  assert.strictEqual(fs.existsSync(missing), false);
  assert.deepStrictEqual(exists.types(), ['start', 'end', 'start', 'end']);
  assert.strictEqual(exists.events[1].context.result, true);
  assert.strictEqual(exists.events[3].context.result, false);
  exists.stop();
}

{
  // A subscriber that changes `context.args` does not change the call.
  const channel = dc.tracingChannel('fs.stat');
  const onStart = common.mustCall((context) => {
    context.args[0] = tmpdir.resolve('does-not-exist');
  });
  channel.start.subscribe(onStart);
  assert.ok(fs.statSync(file).isFile());
  channel.start.unsubscribe(onStart);
}

{
  const rename = record('rename');
  const dest = tmpdir.resolve('renamed.txt');
  fs.renameSync(file, dest);
  assert.deepStrictEqual(rename.types(), ['start', 'end']);
  assert.strictEqual(rename.events[0].context.path, file);
  assert.strictEqual(rename.events[0].context.dest, dest);
  rename.stop();
}
