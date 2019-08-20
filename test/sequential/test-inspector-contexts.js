'use strict';

// Flags: --expose-gc

const common = require('../common');
common.skipIfInspectorDisabled();

const { strictEqual } = require('assert');
const { createContext, runInNewContext } = require('vm');
const { Session } = require('inspector');

const session = new Session();
session.connect();

function notificationPromise(method) {
  return new Promise((resolve) => session.once(method, resolve));
}

async function testContextCreatedAndDestroyed() {
  console.log('Testing context created/destroyed notifications');
  {
    const mainContextPromise =
        notificationPromise('Runtime.executionContextCreated');

    session.post('Runtime.enable');
    const contextCreated = await mainContextPromise;
    const { name, origin, auxData } = contextCreated.params.context;
    if (common.isSunOS || common.isWindows) {
      // uv_get_process_title() is unimplemented on Solaris-likes, it returns
      // an empty string.  On the Windows CI buildbots it returns
      // "Administrator: Windows PowerShell[42]" because of a GetConsoleTitle()
      // quirk. Not much we can do about either, just verify that it contains
      // the PID.
      strictEqual(name.includes(`[${process.pid}]`), true);
    } else {
      let expects = `${process.argv0}[${process.pid}]`;
      if (!common.isMainThread) {
        expects = `Worker[${require('worker_threads').threadId}]`;
      }
      strictEqual(expects, name);
    }
    strictEqual(origin, '',
                JSON.stringify(contextCreated));
    strictEqual(auxData.isDefault, true,
                JSON.stringify(contextCreated));
  }

  {
    const vmContextCreatedPromise =
        notificationPromise('Runtime.executionContextCreated');

    let contextDestroyed = null;
    session.once('Runtime.executionContextDestroyed',
                 (notification) => contextDestroyed = notification);

    runInNewContext('1 + 1');

    const contextCreated = await vmContextCreatedPromise;
    const { id, name, origin, auxData } = contextCreated.params.context;
    strictEqual(name, 'VM Context 1',
                JSON.stringify(contextCreated));
    strictEqual(origin, '',
                JSON.stringify(contextCreated));
    strictEqual(auxData.isDefault, false,
                JSON.stringify(contextCreated));

    // GC is unpredictable...
    while (!contextDestroyed)
      global.gc();

    strictEqual(contextDestroyed.params.executionContextId, id,
                JSON.stringify(contextDestroyed));
  }

  {
    const vmContextCreatedPromise =
        notificationPromise('Runtime.executionContextCreated');

    let contextDestroyed = null;
    session.once('Runtime.executionContextDestroyed',
                 (notification) => contextDestroyed = notification);

    runInNewContext('1 + 1', {}, {
      contextName: 'Custom context',
      contextOrigin: 'https://origin.example'
    });

    const contextCreated = await vmContextCreatedPromise;
    const { name, origin, auxData } = contextCreated.params.context;
    strictEqual(name, 'Custom context',
                JSON.stringify(contextCreated));
    strictEqual(origin, 'https://origin.example',
                JSON.stringify(contextCreated));
    strictEqual(auxData.isDefault, false,
                JSON.stringify(contextCreated));

    // GC is unpredictable...
    while (!contextDestroyed)
      global.gc();
  }

  {
    const vmContextCreatedPromise =
        notificationPromise('Runtime.executionContextCreated');

    let contextDestroyed = null;
    session.once('Runtime.executionContextDestroyed',
                 (notification) => contextDestroyed = notification);

    createContext({}, { origin: 'https://nodejs.org' });

    const contextCreated = await vmContextCreatedPromise;
    const { name, origin, auxData } = contextCreated.params.context;
    strictEqual(name, 'VM Context 2',
                JSON.stringify(contextCreated));
    strictEqual(origin, 'https://nodejs.org',
                JSON.stringify(contextCreated));
    strictEqual(auxData.isDefault, false,
                JSON.stringify(contextCreated));

    // GC is unpredictable...
    while (!contextDestroyed)
      global.gc();
  }

  {
    const vmContextCreatedPromise =
        notificationPromise('Runtime.executionContextCreated');

    let contextDestroyed = null;
    session.once('Runtime.executionContextDestroyed',
                 (notification) => contextDestroyed = notification);

    createContext({}, { name: 'Custom context 2' });

    const contextCreated = await vmContextCreatedPromise;
    const { name, auxData } = contextCreated.params.context;
    strictEqual(name, 'Custom context 2',
                JSON.stringify(contextCreated));
    strictEqual(auxData.isDefault, false,
                JSON.stringify(contextCreated));

    // GC is unpredictable...
    while (!contextDestroyed)
      global.gc();
  }
}

async function testBreakpointHit() {
  console.log('Testing breakpoint is hit in a new context');
  session.post('Debugger.enable');

  const pausedPromise = notificationPromise('Debugger.paused');
  runInNewContext('debugger', {});
  await pausedPromise;
}

testContextCreatedAndDestroyed().then(testBreakpointHit);
