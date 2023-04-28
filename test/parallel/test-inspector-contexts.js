'use strict';

// Flags: --expose-gc

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const vm = require('vm');
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

    session.post('Runtime.enable', assert.ifError);
    const contextCreated = await mainContextPromise;
    const { name, origin, auxData } = contextCreated.params.context;
    if (common.isSunOS || common.isWindows || common.isIBMi) {
      // uv_get_process_title() is unimplemented on Solaris-likes and IBMi,
      // it returns an empty string.  On the Windows CI buildbots it returns
      // "Administrator: Windows PowerShell[42]" because of a GetConsoleTitle()
      // quirk. Not much we can do about either, just verify that it contains
      // the PID.
      assert.strictEqual(name.includes(`[${process.pid}]`), true);
    } else {
      let expects = `${process.argv0}[${process.pid}]`;
      if (!common.isMainThread) {
        expects = `Worker[${require('worker_threads').threadId}]`;
      }
      assert.strictEqual(expects, name);
    }
    assert.strictEqual(origin, '',
                       JSON.stringify(contextCreated));
    assert.strictEqual(auxData.isDefault, true,
                       JSON.stringify(contextCreated));
  }

  {
    const vmContextCreatedPromise =
        notificationPromise('Runtime.executionContextCreated');

    let contextDestroyed = null;
    session.once('Runtime.executionContextDestroyed',
                 (notification) => contextDestroyed = notification);

    vm.runInNewContext('1 + 1');

    const contextCreated = await vmContextCreatedPromise;
    const { id, name, origin, auxData } = contextCreated.params.context;
    assert.strictEqual(name, 'VM Context 1',
                       JSON.stringify(contextCreated));
    assert.strictEqual(origin, '',
                       JSON.stringify(contextCreated));
    assert.strictEqual(auxData.isDefault, false,
                       JSON.stringify(contextCreated));

    // GC is unpredictable...
    console.log('Checking/waiting for GC.');
    while (!contextDestroyed)
      global.gc();
    console.log('Context destroyed.');

    assert.strictEqual(contextDestroyed.params.executionContextId, id,
                       JSON.stringify(contextDestroyed));
  }

  {
    const vmContextCreatedPromise =
        notificationPromise('Runtime.executionContextCreated');

    let contextDestroyed = null;
    session.once('Runtime.executionContextDestroyed',
                 (notification) => contextDestroyed = notification);

    vm.runInNewContext('1 + 1', {}, {
      contextName: 'Custom context',
      contextOrigin: 'https://origin.example'
    });

    const contextCreated = await vmContextCreatedPromise;
    const { name, origin, auxData } = contextCreated.params.context;
    assert.strictEqual(name, 'Custom context',
                       JSON.stringify(contextCreated));
    assert.strictEqual(origin, 'https://origin.example',
                       JSON.stringify(contextCreated));
    assert.strictEqual(auxData.isDefault, false,
                       JSON.stringify(contextCreated));

    // GC is unpredictable...
    console.log('Checking/waiting for GC again.');
    while (!contextDestroyed)
      global.gc();
    console.log('Other context destroyed.');
  }

  {
    const vmContextCreatedPromise =
        notificationPromise('Runtime.executionContextCreated');

    let contextDestroyed = null;
    session.once('Runtime.executionContextDestroyed',
                 (notification) => contextDestroyed = notification);

    vm.createContext({}, { origin: 'https://nodejs.org' });

    const contextCreated = await vmContextCreatedPromise;
    const { name, origin, auxData } = contextCreated.params.context;
    assert.strictEqual(name, 'VM Context 2',
                       JSON.stringify(contextCreated));
    assert.strictEqual(origin, 'https://nodejs.org',
                       JSON.stringify(contextCreated));
    assert.strictEqual(auxData.isDefault, false,
                       JSON.stringify(contextCreated));

    // GC is unpredictable...
    console.log('Checking/waiting for GC a third time.');
    while (!contextDestroyed)
      global.gc();
    console.log('Context destroyed once again.');
  }

  {
    const vmContextCreatedPromise =
        notificationPromise('Runtime.executionContextCreated');

    let contextDestroyed = null;
    session.once('Runtime.executionContextDestroyed',
                 (notification) => contextDestroyed = notification);

    vm.createContext({}, { name: 'Custom context 2' });

    const contextCreated = await vmContextCreatedPromise;
    const { name, auxData } = contextCreated.params.context;
    assert.strictEqual(name, 'Custom context 2',
                       JSON.stringify(contextCreated));
    assert.strictEqual(auxData.isDefault, false,
                       JSON.stringify(contextCreated));

    // GC is unpredictable...
    console.log('Checking/waiting for GC a fourth time.');
    while (!contextDestroyed)
      global.gc();
    console.log('Context destroyed a fourth time.');
  }
}

async function testBreakpointHit() {
  console.log('Testing breakpoint is hit in a new context');
  session.post('Debugger.enable', assert.ifError);

  const pausedPromise = notificationPromise('Debugger.paused');
  vm.runInNewContext('debugger', {});
  await pausedPromise;
}

(async function() {
  await testContextCreatedAndDestroyed();
  await testBreakpointHit();
})().then(common.mustCall());
