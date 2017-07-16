'use strict';
const common = require('../common');
const { fires, neverFires } = common;
common.skipIfInspectorDisabled();
const assert = require('assert');
const {
  Session,
  attachContext,
  detachContext,
  contextAttached
} = require('inspector');
const { promisify } = require('util');
const { createContext } = require('vm');

common.crashOnUnhandledRejection();

const session = new Session();
const once = common.cancellableOnce.bind(session);
const post = promisify(session.post).bind(session);

async function main() {
  session.connect();

  // Basic tests.
  {
    const VM_CONTEXT_NAME = 'New VM Context';
    const VM_CONTEXT_ORIGIN = 'https://nodejs.org/api/vm.html';

    const sandbox = {
      propInSandbox: 42
    };
    createContext(sandbox);

    // Enable context tracking; receive info about default context.
    {
      const eventPromise = fires(once('Runtime.executionContextCreated'));
      post('Runtime.enable');
      const topContext = (await eventPromise).params.context;
      assert.strictEqual(topContext.name, 'Node.js Main Context');
      assert.strictEqual(topContext.origin, '');
      assert.strictEqual(topContext.auxData.isDefault, true);
    }

    let vmContext;

    // Attaching contextified sandbox.
    {
      const eventPromise = fires(once('Runtime.executionContextCreated'));
      attachContext(sandbox, {
        name: VM_CONTEXT_NAME,
        origin: VM_CONTEXT_ORIGIN
      });
      vmContext = (await eventPromise).params.context;
      assert.strictEqual(vmContext.name, VM_CONTEXT_NAME);
      assert.strictEqual(vmContext.origin, VM_CONTEXT_ORIGIN);
      assert.strictEqual(vmContext.auxData.isDefault, false);
      assert.strictEqual(contextAttached(sandbox), true);
    }

    // Evaluating expressions in the specified context.
    {
      const { result } = await post('Runtime.evaluate', {
        expression: 'propInSandbox++',
        contextId: vmContext.id
      });
      assert.strictEqual(result.type, 'number');
      assert.strictEqual(result.value, 42);
      assert.strictEqual(sandbox.propInSandbox, 43);
    }

    // Trying to attach the same context twice.
    {
      const eventPromise =
          neverFires(once('Runtime.executionContextCreated'));
      attachContext(sandbox, {
        name: VM_CONTEXT_NAME,
        origin: VM_CONTEXT_ORIGIN
      });
      await eventPromise;
      assert.strictEqual(contextAttached(sandbox), true);
    }

    // Detach context.
    {
      const eventPromise = fires(once('Runtime.executionContextDestroyed'));
      detachContext(sandbox);
      const destroyedId = (await eventPromise).params.executionContextId;
      assert.strictEqual(destroyedId, vmContext.id);
      assert.strictEqual(contextAttached(sandbox), false);
    }

    // Trying to detach multiple times.
    {
      const eventPromise =
          neverFires(once('Runtime.executionContextDestroyed'));
      detachContext(sandbox);
      await eventPromise;
      assert.strictEqual(contextAttached(sandbox), false);
    }
  }

  // Default context names.
  {
    const contexts = [
      createContext(),
      createContext(),
      createContext()
    ];
    const ids = new Map();
    for (const [i, context] of contexts.entries()) {
      const eventPromise = fires(once('Runtime.executionContextCreated'));
      attachContext(context);
      const info = (await eventPromise).params.context;
      assert.strictEqual(info.name, `vm Module Context ${i + 1}`);
      assert.strictEqual(info.origin, '');
      assert.strictEqual(info.auxData.isDefault, false);
      ids.set(context, info.id);
    }
    for (const context of contexts) {
      const eventPromise = fires(once('Runtime.executionContextDestroyed'));
      detachContext(context);
      const destroyedId = (await eventPromise).params.executionContextId;
      assert.strictEqual(destroyedId, ids.get(context));
    }
  }

  session.disconnect();
}

main();
