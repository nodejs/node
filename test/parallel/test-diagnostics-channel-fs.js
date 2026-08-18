'use strict';

const common = require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const fs = require('node:fs');
const fsp = require('node:fs/promises');
const tmpdir = require('node:os').tmpdir();
const { join } = require('node:path');

const events = [];
const eventTypes = ['start', 'end', 'asyncStart', 'asyncEnd', 'error'];
for (const type of eventTypes) {
  dc.channel(`tracing:fs.operation:${type}`).subscribe((event) => {
    events.push({ type, event });
  });
}

const target = join(tmpdir, `node-test-diagnostics-channel-fs-${process.pid}-${Date.now()}`);
const source = join(tmpdir, `node-test-diagnostics-channel-fs-src-${process.pid}-${Date.now()}`);

function byOperation(operation) {
  return events.filter((e) => e.event.operation === operation);
}

// Sync operations publish start/end (or error) with api: 'sync'.
fs.writeFileSync(target, 'hello');

const openSync = byOperation('open');
assert.ok(openSync.length >= 2, 'expected open start/end for writeFileSync');
const openStart = openSync[0];
const openEnd = openSync[openSync.length - 1];
assert.strictEqual(openStart.type, 'start');
assert.strictEqual(openStart.event.api, 'sync');
assert.strictEqual(openStart.event.path, target);
assert.strictEqual(openStart.event.operation, 'open');
assert.strictEqual(openEnd.type, 'end');
assert.strictEqual(openEnd.event.api, 'sync');
assert.strictEqual(openEnd.event.path, target);
assert.strictEqual(typeof openEnd.event.result, 'number');  // the returned fd

// Callback operations publish start, end, asyncStart, asyncEnd with api.
fs.readFile(target, 'utf8', common.mustSucceed((data) => {
  assert.strictEqual(data, 'hello');

  const readEvents = byOperation('read');
  assert.ok(readEvents.length >= 4, 'expected read start/end/asyncStart/asyncEnd');
  const readStart = readEvents[0];
  const readEnd = readEvents[1];
  const readAsyncStart = readEvents[2];
  const readAsyncEnd = readEvents[readEvents.length - 1];

  assert.strictEqual(readStart.type, 'start');
  assert.strictEqual(readStart.event.api, 'callback');
  assert.strictEqual(readStart.event.fd, undefined);
  assert.strictEqual(readEnd.type, 'end');
  assert.strictEqual(readEnd.event.api, 'callback');
  assert.strictEqual(typeof readEnd.event.fd, 'number');
  assert.strictEqual(readEnd.event.result, undefined);
  assert.strictEqual(readAsyncStart.type, 'asyncStart');
  assert.strictEqual(readAsyncStart.event.api, 'callback');
  assert.strictEqual(typeof readAsyncStart.event.fd, 'number');
  assert.strictEqual(readAsyncEnd.type, 'asyncEnd');
  assert.strictEqual(readAsyncEnd.event.api, 'callback');
  assert.strictEqual(typeof readAsyncEnd.event.fd, 'number');
  assert.strictEqual(readAsyncEnd.event.result, data.length);  // bytes read

  // Promise operations publish the same family with api: 'promise'.
  fsp.stat(target).then(common.mustCall((stats) => {
    assert.strictEqual(typeof stats.size, 'number');

    const statEvents = byOperation('stat');
    assert.ok(statEvents.length >= 4, 'expected stat start/end/asyncStart/asyncEnd');
    const statAsyncEnd = statEvents[statEvents.length - 1];
    assert.strictEqual(statAsyncEnd.type, 'asyncEnd');
    assert.strictEqual(statAsyncEnd.event.api, 'promise');
    assert.strictEqual(statAsyncEnd.event.path, target);
    assert.ok('result' in statAsyncEnd.event);

    // Destination operations carry the `dest` field.
    fs.rename(target, source, common.mustSucceed(() => {
      const renameEvents = byOperation('rename');
      assert.ok(renameEvents.length >= 4, 'expected rename start/end/asyncStart/asyncEnd');
      const renameAsyncEnd = renameEvents[renameEvents.length - 1];
      assert.strictEqual(renameAsyncEnd.type, 'asyncEnd');
      assert.strictEqual(renameAsyncEnd.event.path, target);
      assert.strictEqual(renameAsyncEnd.event.dest, source);

      // Failed operations publish an `error` event carrying the error object.
      fs.readFile('/nonexistent-node-diagnostics-channel-fs', common.mustCall((err2) => {
        assert.ok(err2);
        const errorEvents = events.filter((e) => e.type === 'error');
        const error = errorEvents[errorEvents.length - 1];
        assert.ok(error, 'expected an error event');
        assert.strictEqual(error.event.api, 'callback');
        assert.strictEqual(error.event.operation, 'open');
        assert.strictEqual(error.event.path, '/nonexistent-node-diagnostics-channel-fs');
        assert.strictEqual(error.event.error.code, 'ENOENT');

        fs.rm(source, { force: true }, common.mustCall());
      }));
    }));
  }));
}));
