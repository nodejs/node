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
  {
    const { name } = contextCreated.params.context;
    if (common.isSunOS || common.isWindows) {
      // uv_get_process_title() is unimplemented on Solaris-likes, it returns
      // an empy string.  On the Windows CI buildbots it returns "Administrator:
      // Windows PowerShell[42]" because of a GetConsoleTitle() quirk. Not much
      // we can do about either, just verify that it contains the PID.
      strictEqual(name.includes(`[${process.pid}]`), true);
    } else {
      strictEqual(`${process.argv0}[${process.pid}]`, name);
    }
  }

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
