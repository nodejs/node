// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const { Session } = require('inspector');
const { registerDomainHandlerClass, SessionTerminatedSymbol } =
    require('internal/inspector_node_domains');
const util = require('util');

let handler;

class TestDomainHandler {
  constructor(notificationsChannel) {
    this.notificationsChannel = notificationsChannel;
    this.field = 0;
    this.done = false;
    handler = this;
  }

  methodA(params) {
    return params.a + params.b;
  }

  methodB(params) {
    throw new Error('Booom!');
  }

  methodC(params) {
    return (callback) => this.cb = callback;
  }

  methodD() {
    const a = this.field++;
    return { a };
  }

  _privateMethod() {
    return true;
  }

  [SessionTerminatedSymbol]() {
    this.done = true;
  }
}

async function test() {
  const session = new Session();
  session.connect();
  const post = util.promisify(session.post.bind(session));
  try {
    await post('Test.methodA');
    assert.fail('Exception must have been thrown');
  } catch (e) {
    assert.strictEqual(-32601, e.code);
  }

  registerDomainHandlerClass('Test', TestDomainHandler);
  assert.strictEqual(3, await post('Test.methodA', { a: 1, b: 2 }));
  try {
    await post('Test.methodB');
    assert.fail();
  } catch (e) {
    assert.strictEqual('Booom!', e);
  }
  let result = null;
  session.post('Test.methodC', (error, params) => {
    result = { error, params };
  });
  assert.strictEqual(null, result);
  handler.cb(null, 5);
  assert.deepStrictEqual({ error: null, params: 5 }, result);

  let notification = null;
  session.on('Test.notification1', (n) => notification = n);
  assert.strictEqual(null, notification);

  handler.notificationsChannel('notification1', { c: 8 });
  assert.deepStrictEqual({ method: 'Test.notification1', params: { c: 8 } },
                         notification);
  assert.deepStrictEqual({ a: 0 }, await post('Test.methodD'));
  assert.deepStrictEqual({ a: 1 }, await post('Test.methodD'));
  assert.deepStrictEqual({ a: 2 }, await post('Test.methodD'));

  assert(!handler.done);
  session.disconnect();
  assert(handler.done);

  const session1 = new Session();
  session1.connect();
  const post1 = util.promisify(session1.post.bind(session1));
  assert.deepStrictEqual({ a: 0 }, await post1('Test.methodD'));
  assert.deepStrictEqual({ a: 1 }, await post1('Test.methodD'));
  assert.deepStrictEqual({ a: 2 }, await post1('Test.methodD'));
  await common.assertRejected(post1('Test._privateMethod'));
}

common.crashOnUnhandledRejection();

test();
