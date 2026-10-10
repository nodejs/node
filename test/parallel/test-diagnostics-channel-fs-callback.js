'use strict';

// Checks the events that callback fs functions publish, and that the
// user callback runs between asyncStart and asyncEnd.

const common = require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const fs = require('node:fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const file = tmpdir.resolve('file.txt');
const missing = tmpdir.resolve('missing.txt');
fs.writeFileSync(file, 'hello');

function record(name, log) {
  const channel = dc.tracingChannel(`fs.${name}`);
  const handlers = {};
  for (const type of ['start', 'end', 'asyncStart', 'asyncEnd', 'error']) {
    handlers[type] = (context) => log.push({ type, context });
  }
  channel.subscribe(handlers);
  return () => channel.unsubscribe(handlers);
}

function types(log) {
  return log.map((e) => e.type);
}

function assertSameContext(log) {
  const contexts = log.filter((e) => e.context).map((e) => e.context);
  for (const context of contexts) {
    assert.strictEqual(context, contexts[0]);
  }
}

function testStat() {
  const log = [];
  const stop = record('stat', log);
  fs.stat(file, common.mustSucceed((stats) => {
    log.push({ type: 'callback' });
    process.nextTick(() => {
      stop();
      assert.deepStrictEqual(types(log),
                             ['start', 'end', 'asyncStart', 'callback', 'asyncEnd']);
      assertSameContext(log);
      const { context } = log[0];
      assert.strictEqual(context.api, 'callback');
      assert.strictEqual(context.path, file);
      assert.strictEqual(context.result, stats);
      assert.ok(stats instanceof fs.Stats);
      testStatError();
    });
  }));
  // The callback has not run yet.
  assert.deepStrictEqual(types(log), ['start', 'end']);
}

function testStatError() {
  const log = [];
  const stop = record('stat', log);
  fs.stat(missing, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ENOENT');
    log.push({ type: 'callback' });
    process.nextTick(() => {
      stop();
      assert.deepStrictEqual(
        types(log),
        ['start', 'end', 'error', 'asyncStart', 'callback', 'asyncEnd']);
      assertSameContext(log);
      assert.strictEqual(log[0].context.error, err);
      testRead();
    });
  }));
}

function testRead() {
  const log = [];
  const stop = record('read', log);
  const fd = fs.openSync(file, 'r');
  const buffer = Buffer.alloc(5);
  fs.read(fd, buffer, 0, 5, 0, common.mustSucceed((bytesRead) => {
    log.push({ type: 'callback' });
    process.nextTick(() => {
      stop();
      fs.closeSync(fd);
      assert.deepStrictEqual(types(log),
                             ['start', 'end', 'asyncStart', 'callback', 'asyncEnd']);
      const { context } = log[0];
      assert.strictEqual(context.fd, fd);
      assert.strictEqual(context.args[1], buffer);
      assert.strictEqual(context.result, bytesRead);
      assert.strictEqual(bytesRead, 5);
      testRename();
    });
  }));
}

function testRename() {
  const log = [];
  const stop = record('rename', log);
  const dest = tmpdir.resolve('renamed.txt');
  fs.rename(file, dest, common.mustSucceed(() => {
    process.nextTick(() => {
      stop();
      assert.deepStrictEqual(types(log), ['start', 'end', 'asyncStart', 'asyncEnd']);
      assert.strictEqual(log[0].context.path, file);
      assert.strictEqual(log[0].context.dest, dest);
      fs.renameSync(dest, file);
      testExists();
    });
  }));
}

function testExists() {
  const log = [];
  const stop = record('exists', log);
  const callback = common.mustCall((exists) => {
    assert.strictEqual(exists, true);
    process.nextTick(() => {
      stop();
      assert.deepStrictEqual(types(log), ['start', 'end', 'asyncStart', 'asyncEnd']);
      assert.strictEqual(log[0].context.result, true);
      assert.deepStrictEqual(log[0].context.args, [file, callback]);
      testReadFile();
    });
  });
  fs.exists(file, callback);
}

function testReadFile() {
  const log = [];
  const stop = record('readFile', log);
  fs.readFile(file, 'utf8', common.mustSucceed((data) => {
    process.nextTick(() => {
      stop();
      assert.deepStrictEqual(types(log), ['start', 'end', 'asyncStart', 'asyncEnd']);
      assert.strictEqual(log[0].context.path, file);
      assert.strictEqual(log[0].context.result, data);
      assert.strictEqual(data, 'hello');
      testCloseWithoutCallback();
    });
  }));
}

function testCloseWithoutCallback() {
  const fd = fs.openSync(file, 'r');
  const channel = dc.tracingChannel('fs.close');
  const handlers = {
    start: common.mustCall((context) => {
      assert.strictEqual(context.api, 'callback');
      assert.deepStrictEqual(context.args, [fd]);
    }),
    asyncEnd: common.mustCall(() => {
      process.nextTick(() => channel.unsubscribe(handlers));
    }),
  };
  channel.subscribe(handlers);
  fs.close(fd);
}

testStat();
