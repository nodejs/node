// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const inspector = require('node:inspector/promises');
const { Network } = require('node:inspector');
const test = require('node:test');
const assert = require('node:assert');
const { waitUntil } = require('../common/inspector-helper');
const { setImmediate: waitForTurn } = require('node:timers/promises');

const session = new inspector.Session();
session.connect();

function createRequestPayload(overrides = {}) {
  return {
    requestId: '1',
    timestamp: 1,
    wallTime: 1,
    request: {
      url: 'https://example.com',
      method: 'GET',
      headers: {
        mKey: 'mValue',
      },
    },
    ...overrides,
  };
}

async function assertInvalidInitiatorStack(stack, requestId) {
  session.removeAllListeners();
  await session.post('Network.enable');

  session.on('Network.requestWillBeSent', common.mustNotCall());

  assert.throws(() => {
    Network.requestWillBeSent(createRequestPayload({
      requestId,
      initiator: {
        type: 'script',
        stack,
      },
    }));
  }, {
    name: 'TypeError',
    message: 'Invalid initiator.stack in event',
  });

  await waitForTurn();
}

test('should emit Network.requestWillBeSent with unicode', async () => {
  session.removeAllListeners();
  await session.post('Network.enable');
  const expectedValue = 'CJK 汉字 🍱 🧑‍🧑‍🧒‍🧒';

  const requestWillBeSentFuture = waitUntil(session, 'Network.requestWillBeSent')
    .then(([event]) => {
      assert.strictEqual(event.params.request.url, expectedValue);
      assert.strictEqual(event.params.request.method, expectedValue);
      assert.strictEqual(event.params.request.headers.mKey, expectedValue);
    });

  Network.requestWillBeSent(createRequestPayload({
    request: {
      url: expectedValue,
      method: expectedValue,
      headers: {
        mKey: expectedValue,
      },
    },
  }));

  await requestWillBeSentFuture;
});

test('should emit Network.requestWillBeSent with custom initiator', async () => {
  session.removeAllListeners();
  await session.post('Network.enable');

  const requestWillBeSentFuture = waitUntil(session, 'Network.requestWillBeSent')
    .then(([event]) => {
      const { initiator } = event.params;
      assert.strictEqual(initiator.type, 'parser');
      assert.strictEqual(initiator.url, 'node:https://initiator.test/app.js');
      assert.strictEqual(initiator.lineNumber, 12);
      assert.strictEqual(initiator.columnNumber, 34);
      assert.strictEqual(initiator.requestId, 'parent-request-id');
      assert.strictEqual(initiator.stack.description, 'custom stack');
      assert.deepStrictEqual(initiator.stack.callFrames, [{
        functionName: 'run',
        scriptId: '99',
        url: 'file:///custom-frame.js',
        lineNumber: 3,
        columnNumber: 5,
      }]);
      assert.deepStrictEqual(initiator.stack.parent.callFrames, [{
        functionName: 'parentRun',
        scriptId: '100',
        url: 'file:///parent-frame.js',
        lineNumber: 8,
        columnNumber: 13,
      }]);
      assert.deepStrictEqual(initiator.stack.parentId, {
        id: 'async-stack-id',
        debuggerId: 'debugger-1',
      });
    });

  Network.requestWillBeSent(createRequestPayload({
    requestId: 'custom-initiator-request',
    initiator: {
      type: 'parser',
      url: 'node:https://initiator.test/app.js',
      lineNumber: 12,
      columnNumber: 34,
      requestId: 'parent-request-id',
      stack: {
        description: 'custom stack',
        callFrames: [{
          functionName: 'run',
          scriptId: '99',
          url: 'file:///custom-frame.js',
          lineNumber: 3,
          columnNumber: 5,
        }],
        parent: {
          callFrames: [{
            functionName: 'parentRun',
            scriptId: '100',
            url: 'file:///parent-frame.js',
            lineNumber: 8,
            columnNumber: 13,
          }],
          extraNumber: 1.5,
          extraBoolean: true,
          extraNull: null,
        },
        parentId: {
          id: 'async-stack-id',
          debuggerId: 'debugger-1',
        },
        extraArray: ['frame', 1, false, null, { nested: 'value' }],
      },
    },
  }));

  await requestWillBeSentFuture;
});

test('should throw if initiator.type is missing', async () => {
  session.removeAllListeners();
  await session.post('Network.enable');

  session.on('Network.requestWillBeSent', common.mustNotCall());

  assert.throws(() => {
    Network.requestWillBeSent(createRequestPayload({
      requestId: 'missing-initiator-type',
      initiator: {
        stack: {
          callFrames: [],
        },
      },
    }));
  }, {
    name: 'TypeError',
    message: 'Missing initiator.type in event',
  });

  await waitForTurn();
});

test('should throw if initiator.stack is invalid', async () => {
  await assertInvalidInitiatorStack({
    callFrames: [],
    unsupportedValue: 1n,
  }, 'invalid-initiator-stack');
});

test('should throw if initiator.stack contains an invalid array element',
     async () => {
       await assertInvalidInitiatorStack({
         callFrames: [],
         extraArray: [1n],
       }, 'invalid-initiator-stack-array-element');
     });

test('should throw if initiator.stack contains an array accessor that throws',
     async () => {
       const extraArray = [];
       Object.defineProperty(extraArray, 0, {
         enumerable: true,
         get() {
           throw new Error('array getter boom');
         },
       });
       extraArray.length = 1;

       await assertInvalidInitiatorStack({
         callFrames: [],
         extraArray,
       }, 'invalid-initiator-stack-array-getter');
     });

test('should throw if initiator.stack has a property accessor that throws',
     async () => {
       const stack = { callFrames: [] };
       Object.defineProperty(stack, 'broken', {
         enumerable: true,
         get() {
           throw new Error('getter boom');
         },
       });

       await assertInvalidInitiatorStack(
         stack,
         'invalid-initiator-stack-property-getter',
       );
     });

test('should throw if initiator.stack property enumeration throws', async () => {
  const stack = new Proxy({ callFrames: [] }, {
    ownKeys() {
      throw new Error('ownKeys boom');
    },
    getOwnPropertyDescriptor() {
      return {
        enumerable: true,
        configurable: true,
      };
    },
  });

  await assertInvalidInitiatorStack(
    stack,
    'invalid-initiator-stack-own-keys',
  );
});
