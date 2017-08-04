'use strict';
const common = require('../common');
const { fires, neverFires } = common;
common.skipIfInspectorDisabled();
const assert = require('assert');
const {
  Session,
  attachContext,
  detachContext
} = require('inspector');
const { promisify } = require('util');
const {
  Script,
  createContext,
  runInContext,
  runInNewContext,
  runInThisContext
} = require('vm');

common.crashOnUnhandledRejection();

const session = new Session();
const once = common.cancellableOnce.bind(session);
const post = promisify(session.post).bind(session);

async function main() {
  session.connect();

  post('Debugger.enable');

  // Get info about default context.
  let topContext;
  {
    const eventPromise = fires(once('Runtime.executionContextCreated'));
    post('Runtime.enable');
    topContext = (await eventPromise).params.context;
    assert.strictEqual(topContext.auxData.isDefault, true);
  }

  // Create sandbox for use later.
  const sandbox = createContext();
  let vmContext;
  {
    const eventPromise = fires(once('Runtime.executionContextCreated'));
    attachContext(sandbox);
    vmContext = (await eventPromise).params.context;
  }

  // Script
  {
    // Bind to current context by default
    {
      const eventPromise = fires(once('Debugger.scriptParsed'));
      new Script('Array;');
      const { params } = await eventPromise;
      assert.strictEqual(params.executionContextId, topContext.id);
    }

    // Bind to selected context
    {
      const eventPromise = fires(once('Debugger.scriptParsed'));
      new Script('Array;', {
        contextifiedSandbox: sandbox
      });
      const { params } = await eventPromise;
      assert.strictEqual(params.executionContextId, vmContext.id);
    }
  }

  // runInContext: Bind script to specified context.
  {
    const eventPromise = fires(once('Debugger.scriptParsed'));
    runInContext('Array;', sandbox);
    const { params } = await eventPromise;
    assert.strictEqual(params.executionContextId, vmContext.id);
  }

  // runInNewContext: Bind script to new context.
  {
    const createdPromise =
        fires(once('Runtime.executionContextCreated'));
    const destroyedPromise =
        fires(once('Runtime.executionContextDestroyed'));
    const eventPromise = fires(once('Debugger.scriptParsed'));
    runInNewContext('Array;', {});
    await createdPromise;
    const { params } = await eventPromise;
    assert.notStrictEqual(params.executionContextId, topContext.id);
    assert.notStrictEqual(params.executionContextId, vmContext.id);
    await destroyedPromise;
  }

  // runInThisContext: Bind script to current context.
  {
    const eventPromise = fires(once('Debugger.scriptParsed'));
    runInThisContext('Array;');
    const { params } = await eventPromise;
    assert.strictEqual(params.executionContextId, topContext.id);
  }

  session.disconnect();
}

main();
