'use strict';

const common = require('../../common');
common.skipIfInspectorDisabled();

const assert = require('node:assert');
const { once } = require('node:events');
const { Session } = require('node:inspector');

const binding = require(`./build/${common.buildType}/binding`);

const session = new Session();
session.connect();

(async function() {
  const mainContextPromise =
      once(session, 'Runtime.executionContextCreated');
  session.post('Runtime.enable', assert.ifError);
  await mainContextPromise;

  // Addon-created context should be reported to the inspector.
  {
    const addonContextPromise =
        once(session, 'Runtime.executionContextCreated');

    const ctx = binding.makeContext();
    const result = binding.runInContext(ctx, '1 + 1');
    assert.strictEqual(result, 2);

    const { 0: contextCreated } = await addonContextPromise;
    const { name, origin, auxData } = contextCreated.params.context;
    assert.strictEqual(name, 'Addon Context',
                       JSON.stringify(contextCreated));
    assert.strictEqual(origin, 'addon://about',
                       JSON.stringify(contextCreated));
    assert.strictEqual(auxData.isDefault, false,
                       JSON.stringify(contextCreated));
  }

  // `debugger` statement should pause in addon-created context.
  {
    session.post('Debugger.enable', assert.ifError);

    const pausedPromise = once(session, 'Debugger.paused');
    const ctx = binding.makeContext();
    binding.runInContext(ctx, 'debugger');
    await pausedPromise;

    session.post('Debugger.resume');
  }
})().then(common.mustCall());
