// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const { Session } = require('inspector');
const { newTarget } = require('internal/inspector_node_domains');
const util = require('util');

const targets = new Set();

class Target {
  constructor(suffix, autoconnect) {
    this._type = 'node';
    this._title = `Test target ${suffix}`;
    this._url = `ws://localhost/id/${suffix}`;
    this._attachCallback = null;
    this._sessions = new Set();
    this.targetAttached = false;
    this.messages = [];
    this._autoconnect = !!autoconnect;
    const { id, destroyed, updated } =
        newTarget(this._type, this._title, this._url, this.attach.bind(this));
    this.id = id;
    this._destroyed = destroyed;
    this._updatedCb = updated;
    targets.add(this);
  }

  destroyed() {
    targets.delete(this);
    this._destroyed();
  }

  attach(attachCallback, messageCallback) {
    this._attachCallback = attachCallback;
    this._messageCallback = messageCallback;
    if (this._autoconnect)
      this.attached(false);
  }

  attached(waitingForDebugger) {
    this._attachCallback(
      null,
      waitingForDebugger,
      (message, callback) => {
        this.messages.push(message);
        callback();
      },
      (callback) => {
        assert.ok(this.targetAttached, 'Should be attached');
        this.targetAttached = false;
        callback();
      });
    this.targetAttached = true;
  }

  detach() {
    assert(this.targetAttached);
    this.targetAttached = false;
    this._messageCallback(null);
  }

  toInfo(attachedOverride) {
    const attached =
        attachedOverride === undefined ? this.targetAttached : attachedOverride;
    return {
      targetId: this.id,
      attached,
      type: this._type,
      title: this._title,
      url: this._url
    };
  }

  updateTitle(title) {
    this._title = title;
    this._updatedCb(null, title);
  }

  postMessage(message) {
    this._messageCallback(message);
  }
}

const session = new Session();
const post = util.promisify(session.post.bind(session));

function collectNotifications(session, event, array) {
  function listener(notification) {
    array.push(notification);
  }
  session.addListener(event, listener);
  return function() {
    session.removeListener(event, listener);
  };
}

function clearTargets() {
  for (const target of targets) {
    target.destroyed();
  }
}

function _t(notifications) {
  return notifications.map((notification) => notification.params.targetInfo);
}

async function testTargetsDiscovery() {
  console.log('Testing targets discovery');
  const targetsCreated = [];
  const targetsDestroyed = [];

  const listeners = [
    collectNotifications(session, 'Target.targetCreated', targetsCreated),
    collectNotifications(session, 'Target.targetDestroyed', targetsDestroyed)
  ];

  assert.deepStrictEqual([], await post('Target.getTargets'));
  const target1 = new Target('a');
  assert.deepStrictEqual([ target1.toInfo() ],
                         await post('Target.getTargets'));
  assert.deepStrictEqual([], _t(targetsCreated));
  await post('Target.setDiscoverTargets', { discover: true });
  assert.deepStrictEqual([], _t(targetsCreated));
  const target2 = new Target('b');
  assert.deepStrictEqual([ target2.toInfo() ], _t(targetsCreated));
  await post('Target.setDiscoverTargets', { discover: false });
  const target3 = new Target('c');
  assert.deepStrictEqual([ target2.toInfo() ], _t(targetsCreated));
  assert.deepStrictEqual([
    target1.toInfo(), target2.toInfo(), target3.toInfo()
  ], await post('Target.getTargets'));
  const target = await post('Target.getTargetInfo', { targetId: target3.id });
  assert.deepStrictEqual(target3.toInfo(), target);
  await common.assertRejected(
    post('Target.getTargetInfo', { targetId: 'bogus' }),
    'Should have reported no such target');

  target2.destroyed();
  assert.deepStrictEqual([], _t(targetsDestroyed));
  assert.deepStrictEqual([
    target1.toInfo(), target3.toInfo()
  ], await post('Target.getTargets'));
  await post('Target.setDiscoverTargets', { discover: true });
  target3.destroyed();
  assert.deepStrictEqual([ target3.toInfo() ], _t(targetsDestroyed));
  listeners.forEach((fn) => fn());
}

async function testAttach() {
  console.log('Testing attaching to a target');
  const changed = [];
  const attachedTargets = [];
  const detachedTargets = [];
  const destroyed = [];

  const listeners = [
    collectNotifications(session, 'Target.targetInfoChanged', changed),
    collectNotifications(session, 'Target.attachedToTarget', attachedTargets),
    collectNotifications(session, 'Target.detachedFromTarget', detachedTargets),
    collectNotifications(session, 'Target.targetDestroyed', destroyed)
  ];

  const target = new Target('attachable');
  let wasAttached = false;
  const promise = post('Target.attachToTarget', { targetId: target.id })
                      .then((result) => {
                        wasAttached = true;
                        return result;
                      });
  assert.ok(!wasAttached);
  assert.deepStrictEqual(target.toInfo(),
                         await post('Target.getTargetInfo',
                                    { targetId: target.id }));

  assert.deepStrictEqual([], changed);
  assert.deepStrictEqual([], attachedTargets);

  const waitingForDebugger = false;

  target.attached(waitingForDebugger);
  const { sessionId } = await promise;
  assert.ok(wasAttached);
  assert.deepStrictEqual(target.toInfo(),
                         await post('Target.getTargetInfo',
                                    { targetId: target.id }));
  assert.deepStrictEqual(
    { sessionId, targetInfo: target.toInfo(), waitingForDebugger },
    attachedTargets[0].params);
  assert.deepStrictEqual(target.toInfo(), changed[0].params.targetInfo);

  target.destroyed();

  assert.deepStrictEqual(target.toInfo(false), destroyed[0].params.targetInfo);
  assert.deepStrictEqual(target.toInfo(false), changed[1].params.targetInfo);
  assert.deepStrictEqual({ sessionId }, detachedTargets[0].params);
  listeners.forEach((fn) => fn());
}

async function testMessages() {
  console.log('Testing messages passing');
  const detached = [];
  const messages = [];

  const listeners = [
    collectNotifications(session, 'Target.detachedFromTarget', detached),
    collectNotifications(session, 'Target.receivedMessageFromTarget', messages)
  ];

  const target = new Target('message_recipient');
  const promise = post('Target.attachToTarget', { targetId: target.id });
  target.attached(false);
  const { sessionId } = await promise;

  assert.strictEqual(0, messages.length);
  const message = 'Test message';
  target.postMessage(message);
  assert.strictEqual(1, messages.length);
  assert.deepStrictEqual({ sessionId, message }, messages[0].params);

  assert.strictEqual(0, target.messages.length);
  await post('Target.sendMessageToTarget', { sessionId, message });
  assert.deepStrictEqual([message], target.messages);

  assert.strictEqual(0, detached.length);
  await post('Target.detachFromTarget', { targetId: target.id, sessionId });
  assert.deepStrictEqual({ sessionId }, detached[0].params);
  assert.deepStrictEqual(1, detached.length);

  listeners.forEach((fn) => fn());
}

async function testChildSessionsLifetime() {
  console.log('Testing target connection lifetime is linked to parent session');
  const s = new Session();
  const t1 = new Target(1, true);
  const t2 = new Target(2, true);
  s.connect();

  const p = util.promisify(s.post.bind(s));

  await p('Target.attachToTarget', { targetId: t1.id });
  await p('Target.attachToTarget', { targetId: t2.id });

  assert.ok(t1.targetAttached);
  assert.ok(t2.targetAttached);

  s.disconnect();

  assert.ok(!t1.targetAttached);
  assert.ok(!t2.targetAttached);
}

async function testErrors() {
  console.log('Testing errors thrown');
  const s = new Session();
  s.connect();
  const p = util.promisify(s.post.bind(s));

  const target = new Target('exists', true);

  await common.assertRejected(p('Target.getTargetInfo'));
  await common.assertRejected(p('Target.getTargetInfo',
                                { targetId: 'bad' }));

  await common.assertRejected(p('Target.attachToTarget'));
  await common.assertRejected(p('Target.attachToTarget',
                                { targetId: 'bad' }));

  await common.assertRejected(p('Target.detachFromTarget'));
  await common.assertRejected(p('Target.detachFromTarget',
                                { sessionId: 'bad' }));

  await common.assertRejected(p('Target.sendMessageToTarget',
                                { sessionId: 'sid' }));

  const { sessionId } =
      await p('Target.attachToTarget', { targetId: target.id });
  await common.assertRejected(p('Target.sendMessageToTarget',
                                { sessionId }));

  await p('Target.sendMessageToTarget', { sessionId, message: 'amessage' });
  await p('Target.detachFromTarget', { sessionId });
  await common.assertRejected(p('Target.sendMessageToTarget',
                                { sessionId, message: 'amessage' }));

  s.disconnect();
}

async function testAutoAttach() {
  console.log('Testing auto attach');
  const s = new Session();
  s.connect();
  const p = util.promisify(s.post.bind(s));

  const first = new Target('first', true);
  assert(!first.targetAttached);
  await p('Target.setAutoAttach', { autoAttach: true });
  assert(first.targetAttached);
  const second = new Target('second', true);
  assert(second.targetAttached);
  await p('Target.setAutoAttach', { autoAttach: false });
  assert(!first.targetAttached);
  assert(!second.targetAttached);

  const third = new Target('third', true);
  assert(!third.targetAttached);
  s.disconnect();

  const s2 = new Session();
  s2.connect();
  await s2.post('Target.setAutoAttach', { autoAttach: true });
  assert.deepStrictEqual(
    [ true, true, true ],
    [ first, second, third ].map((t) => t.targetAttached));
  s2.disconnect();
}

async function testTitleUpdate() {
  console.log('Test title update');
  const updates = [];
  const s = new Session();
  collectNotifications(s, 'Target.targetInfoChanged', updates),
  s.connect();
  const p = util.promisify(s.post.bind(s));

  const t = new Target('target', true);

  await p('Target.setDiscoverTargets', { discover: true });
  assert.strictEqual(0, updates.length);
  const newTitle = 'new';
  t.updateTitle(newTitle);
  assert.deepStrictEqual([{
    method: 'Target.targetInfoChanged',
    params: { targetInfo: t.toInfo() }
  }], updates);
  t.updateTitle(newTitle);
  assert.strictEqual(1, updates.length);
  s.disconnect();
}

async function testDetach() {
  console.log('Test target may initiate detach');
  const changed = [];
  const detached = [];

  const s = new Session();
  collectNotifications(s, 'Target.targetInfoChanged', changed),
  collectNotifications(s, 'Target.detachedFromTarget', detached),
  s.connect();
  const p = util.promisify(s.post.bind(s));
  await p('Target.setAutoAttach', { autoAttach: true });
  const t = new Target('detachesitself', true);

  assert.strictEqual(true, changed[0].params.targetInfo.attached);
  t.detach();
  assert.strictEqual(2, changed.length);
  assert.strictEqual(false, changed[1].params.targetInfo.attached);
  assert.strictEqual(1, detached.length);
}

async function test() {
  session.connect();
  await testTargetsDiscovery();
  await testAttach();
  await testMessages();
  session.disconnect();
  clearTargets();

  await testChildSessionsLifetime();
  await testErrors();
  await testAutoAttach();
  await testTitleUpdate();
  clearTargets();
  await testDetach();
}

common.crashOnUnhandledRejection();

test();
