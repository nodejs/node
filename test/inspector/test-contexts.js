'use strict';

// Flags: --expose-gc

const common = require('../common');
common.skipIfInspectorDisabled();

const { strictEqual } = require('assert');
const { runInNewContext } = require('vm');
const { Session } = require('inspector');

const session = new Session();
session.connect();

function notificationPromise(method) {
  return new Promise((resolve) => session.once(method, resolve));
}

async function testContextCreatedAndDestroyed() {
  console.log('Testing context created/destroyed notifications');
  const mainContextPromise =
      notificationPromise('Runtime.executionContextCreated');

  session.post('Runtime.enable');
  let contextCreated = await mainContextPromise;
  strictEqual('Node.js Main Context',
              contextCreated.params.context.name,
              JSON.stringify(contextCreated));

  const secondContextCreatedPromise =
      notificationPromise('Runtime.executionContextCreated');

  let contextDestroyed = null;
  session.once('Runtime.executionContextDestroyed',
               (notification) => contextDestroyed = notification);

  runInNewContext('1 + 1', {});

  contextCreated = await secondContextCreatedPromise;
  strictEqual('VM Context 1',
              contextCreated.params.context.name,
              JSON.stringify(contextCreated));

  // GC is unpredictable...
  while (!contextDestroyed)
    global.gc();

  strictEqual(contextCreated.params.context.id,
              contextDestroyed.params.executionContextId,
              JSON.stringify(contextDestroyed));
}

async function testBreakpointHit() {
  console.log('Testing breakpoint is hit in a new context');
  session.post('Debugger.enable');

  const pausedPromise = notificationPromise('Debugger.paused');
  runInNewContext('debugger', {});
  await pausedPromise;
}

common.crashOnUnhandledRejection();
testContextCreatedAndDestroyed().then(testBreakpointHit);
