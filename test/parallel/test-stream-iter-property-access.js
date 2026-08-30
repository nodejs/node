// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  Broadcast,
  Share,
  SyncShare,
  broadcast,
  broadcastProtocol,
  bytes,
  bytesSync,
  drainableProtocol,
  from,
  fromSync,
  ondrain,
  pull,
  pullSync,
  share,
  shareProtocol,
  shareSync,
  shareSyncProtocol,
  toAsyncStreamable,
  toStreamable,
} = require('stream/iter');

function protocolFixture(symbol, result) {
  let accesses = 0;
  const input = {};
  const method = common.mustCall(function() {
    assert.strictEqual(this, input);
    return result;
  });
  Object.defineProperty(input, symbol, {
    get() {
      accesses++;
      if (accesses > 1) throw new Error('protocol method read twice');
      return method;
    },
  });
  return { input, get accesses() { return accesses; } };
}

function statefulTransformFixture() {
  let accesses = 0;
  const transform = {};
  const method = common.mustCall(function(source) {
    assert.strictEqual(this, transform);
    return source;
  });
  Object.defineProperty(transform, 'transform', {
    get() {
      accesses++;
      if (accesses > 1) throw new Error('transform method read twice');
      return method;
    },
  });
  return { transform, get accesses() { return accesses; } };
}

async function testFromSnapshotsProtocolMethods() {
  for (const symbol of [toAsyncStreamable, toStreamable]) {
    const fixture = protocolFixture(symbol, 'abc');
    assert.deepStrictEqual(await bytes(from(fixture.input)),
                           new Uint8Array([97, 98, 99]));
    assert.strictEqual(fixture.accesses, 1);
  }

  const fixture = protocolFixture(toAsyncStreamable, 'nested');
  async function* source() {
    yield fixture.input;
  }
  assert.deepStrictEqual(await bytes(from(source())),
                         new Uint8Array([110, 101, 115, 116, 101, 100]));
  assert.strictEqual(fixture.accesses, 1);
}

async function testFromSyncSnapshotsProtocolMethods() {
  const fixture = protocolFixture(toStreamable, 'abc');
  assert.deepStrictEqual(bytesSync(fromSync(fixture.input)),
                         new Uint8Array([97, 98, 99]));
  assert.strictEqual(fixture.accesses, 1);
}

async function testPullSnapshotsStatefulTransform() {
  const fixture = statefulTransformFixture();
  const controller = new AbortController();
  const readable = pull('abc', fixture.transform, {
    signal: controller.signal,
  });

  assert.deepStrictEqual(await bytes(readable),
                         new Uint8Array([97, 98, 99]));
  assert.strictEqual(fixture.accesses, 1);
}

async function testPullSyncSnapshotsStatefulTransform() {
  const fixture = statefulTransformFixture();
  assert.deepStrictEqual(bytesSync(pullSync('abc', fixture.transform)),
                         new Uint8Array([97, 98, 99]));
  assert.strictEqual(fixture.accesses, 1);
}

async function testMultiConsumerProtocolsSnapshotMethods() {
  const broadcastTarget = broadcast().broadcast;
  const broadcastFixture = protocolFixture(
    broadcastProtocol, broadcastTarget);
  assert.strictEqual(
    Broadcast.from(broadcastFixture.input).broadcast, broadcastTarget);
  assert.strictEqual(broadcastFixture.accesses, 1);

  const shareTarget = share('abc');
  const shareFixture = protocolFixture(shareProtocol, shareTarget);
  assert.strictEqual(Share.from(shareFixture.input), shareTarget);
  assert.strictEqual(shareFixture.accesses, 1);

  const syncShareTarget = shareSync('abc');
  const syncShareFixture = protocolFixture(
    shareSyncProtocol, syncShareTarget);
  assert.strictEqual(SyncShare.fromSync(syncShareFixture.input),
                     syncShareTarget);
  assert.strictEqual(syncShareFixture.accesses, 1);

  broadcastTarget.cancel();
  shareTarget.cancel();
  syncShareTarget.cancel();
}

async function testDrainableProtocolSnapshotMethod() {
  const fixture = protocolFixture(drainableProtocol, true);
  assert.strictEqual(await ondrain(fixture.input), true);
  assert.strictEqual(fixture.accesses, 1);
}

Promise.all([
  testFromSnapshotsProtocolMethods(),
  testFromSyncSnapshotsProtocolMethods(),
  testPullSnapshotsStatefulTransform(),
  testPullSyncSnapshotsStatefulTransform(),
  testMultiConsumerProtocolsSnapshotMethods(),
  testDrainableProtocolSnapshotMethod(),
]).then(common.mustCall());
