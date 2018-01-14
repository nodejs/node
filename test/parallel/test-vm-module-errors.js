'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');
common.crashOnUnhandledRejection();

const assert = require('assert');

const { Module, createContext } = require('vm');

async function expectsRejection(fn, settings) {
  const validateError = common.expectsError(settings);
  // Retain async context.
  const storedError = new Error('Thrown from:');
  try {
    await fn();
  } catch (err) {
    try {
      validateError(err);
    } catch (validationError) {
      console.error(validationError);
      console.error('Original error:');
      console.error(err);
      throw storedError;
    }
    return;
  }
  assert.fail('Missing expected exception');
}

async function createEmptyLinkedModule() {
  const m = new Module('');
  await m.link(common.mustNotCall());
  return m;
}

async function checkArgType() {
  common.expectsError(() => {
    new Module();
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });

  for (const invalidOptions of [
    0, 1, null, true, 'str', () => {}, Symbol.iterator
  ]) {
    common.expectsError(() => {
      new Module('', invalidOptions);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    });
  }

  for (const invalidLinker of [
    0, 1, undefined, null, true, 'str', {}, Symbol.iterator
  ]) {
    await expectsRejection(async () => {
      const m = new Module('');
      await m.link(invalidLinker);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    });
  }
}

// Check methods/properties can only be used under a specific state.
async function checkModuleState() {
  await expectsRejection(async () => {
    const m = new Module('');
    await m.link(common.mustNotCall());
    assert.strictEqual(m.linkingStatus, 'linked');
    await m.link(common.mustNotCall());
  }, {
    code: 'ERR_VM_MODULE_ALREADY_LINKED'
  });

  await expectsRejection(async () => {
    const m = new Module('');
    m.link(common.mustNotCall());
    assert.strictEqual(m.linkingStatus, 'linking');
    await m.link(common.mustNotCall());
  }, {
    code: 'ERR_VM_MODULE_ALREADY_LINKED'
  });

  common.expectsError(() => {
    const m = new Module('');
    m.instantiate();
  }, {
    code: 'ERR_VM_MODULE_NOT_LINKED'
  });

  await expectsRejection(async () => {
    const m = new Module('import "foo";');
    try {
      await m.link(common.mustCall(() => ({})));
    } catch (err) {
      assert.strictEqual(m.linkingStatus, 'errored');
      m.instantiate();
    }
    assert.fail('Unreachable');
  }, {
    code: 'ERR_VM_MODULE_NOT_LINKED'
  });

  {
    const m = new Module('import "foo";');
    await m.link(common.mustCall(async (module, specifier) => {
      assert.strictEqual(module, m);
      assert.strictEqual(specifier, 'foo');
      assert.strictEqual(m.linkingStatus, 'linking');
      common.expectsError(() => {
        m.instantiate();
      }, {
        code: 'ERR_VM_MODULE_NOT_LINKED'
      });
      return new Module('');
    }));
    m.instantiate();
    await m.evaluate();
  }

  await expectsRejection(async () => {
    const m = new Module('');
    await m.evaluate();
  }, {
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must be one of instantiated, evaluated, and errored'
  });

  await expectsRejection(async () => {
    const m = await createEmptyLinkedModule();
    await m.evaluate();
  }, {
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must be one of instantiated, evaluated, and errored'
  });

  common.expectsError(() => {
    const m = new Module('');
    m.error;
  }, {
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must be errored'
  });

  await expectsRejection(async () => {
    const m = await createEmptyLinkedModule();
    m.instantiate();
    await m.evaluate();
    m.error;
  }, {
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must be errored'
  });

  common.expectsError(() => {
    const m = new Module('');
    m.namespace;
  }, {
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must not be uninstantiated or instantiating'
  });

  await expectsRejection(async () => {
    const m = await createEmptyLinkedModule();
    m.namespace;
  }, {
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must not be uninstantiated or instantiating'
  });
}

// Check link() fails when the returned module is not valid.
async function checkLinking() {
  await expectsRejection(async () => {
    const m = new Module('import "foo";');
    try {
      await m.link(common.mustCall(() => ({})));
    } catch (err) {
      assert.strictEqual(m.linkingStatus, 'errored');
      throw err;
    }
    assert.fail('Unreachable');
  }, {
    code: 'ERR_VM_MODULE_NOT_MODULE'
  });

  await expectsRejection(async () => {
    const c = createContext({ a: 1 });
    const foo = new Module('', { context: c });
    await foo.link(common.mustNotCall());
    const bar = new Module('import "foo";');
    try {
      await bar.link(common.mustCall(() => foo));
    } catch (err) {
      assert.strictEqual(bar.linkingStatus, 'errored');
      throw err;
    }
    assert.fail('Unreachable');
  }, {
    code: 'ERR_VM_MODULE_DIFFERENT_CONTEXT'
  });

  await expectsRejection(async () => {
    const erroredModule = new Module('import "foo";');
    try {
      await erroredModule.link(common.mustCall(() => ({})));
    } catch (err) {
      // ignored
    } finally {
      assert.strictEqual(erroredModule.linkingStatus, 'errored');
    }

    const rootModule = new Module('import "errored";');
    await rootModule.link(common.mustCall(() => erroredModule));
  }, {
    code: 'ERR_VM_MODULE_LINKING_ERRORED'
  });
}

// Check the JavaScript engine deals with exceptions correctly
async function checkExecution() {
  await (async () => {
    const m = new Module('import { nonexistent } from "module";');
    await m.link(common.mustCall(() => new Module('')));

    // There is no code for this exception since it is thrown by the JavaScript
    // engine.
    assert.throws(() => {
      m.instantiate();
    }, SyntaxError);
  })();

  await (async () => {
    const m = new Module('throw new Error();');
    await m.link(common.mustNotCall());
    m.instantiate();
    const evaluatePromise = m.evaluate();
    await evaluatePromise.catch(() => {});
    assert.strictEqual(m.status, 'errored');
    try {
      await evaluatePromise;
    } catch (err) {
      assert.strictEqual(m.error, err);
      return;
    }
    assert.fail('Missing expected exception');
  })();
}

const finished = common.mustCall();

(async function main() {
  await checkArgType();
  await checkModuleState();
  await checkLinking();
  await checkExecution();
  finished();
})();
