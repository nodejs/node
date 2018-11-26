'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');

const assert = require('assert');

const { SourceTextModule, createContext } = require('vm');

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
  const m = new SourceTextModule('');
  await m.link(common.mustNotCall());
  return m;
}

async function checkArgType() {
  common.expectsError(() => {
    new SourceTextModule();
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });

  for (const invalidOptions of [
    0, 1, null, true, 'str', () => {}, { url: 0 }, Symbol.iterator,
    { context: null }, { context: 'hucairz' }, { context: {} }
  ]) {
    common.expectsError(() => {
      new SourceTextModule('', invalidOptions);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    });
  }

  for (const invalidLinker of [
    0, 1, undefined, null, true, 'str', {}, Symbol.iterator
  ]) {
    await expectsRejection(async () => {
      const m = new SourceTextModule('');
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
    const m = new SourceTextModule('');
    await m.link(common.mustNotCall());
    assert.strictEqual(m.linkingStatus, 'linked');
    await m.link(common.mustNotCall());
  }, {
    code: 'ERR_VM_MODULE_ALREADY_LINKED'
  });

  await expectsRejection(async () => {
    const m = new SourceTextModule('');
    m.link(common.mustNotCall());
    assert.strictEqual(m.linkingStatus, 'linking');
    await m.link(common.mustNotCall());
  }, {
    code: 'ERR_VM_MODULE_ALREADY_LINKED'
  });

  common.expectsError(() => {
    const m = new SourceTextModule('');
    m.instantiate();
  }, {
    code: 'ERR_VM_MODULE_NOT_LINKED'
  });

  await expectsRejection(async () => {
    const m = new SourceTextModule('import "foo";');
    try {
      await m.link(common.mustCall(() => ({})));
    } catch {
      assert.strictEqual(m.linkingStatus, 'errored');
      m.instantiate();
    }
    assert.fail('Unreachable');
  }, {
    code: 'ERR_VM_MODULE_NOT_LINKED'
  });

  {
    const m = new SourceTextModule('import "foo";');
    await m.link(common.mustCall(async (specifier, module) => {
      assert.strictEqual(module, m);
      assert.strictEqual(specifier, 'foo');
      assert.strictEqual(m.linkingStatus, 'linking');
      common.expectsError(() => {
        m.instantiate();
      }, {
        code: 'ERR_VM_MODULE_NOT_LINKED'
      });
      return new SourceTextModule('');
    }));
    m.instantiate();
    await m.evaluate();
  }

  await expectsRejection(async () => {
    const m = new SourceTextModule('');
    await m.evaluate();
  }, {
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must be one of instantiated, evaluated, and errored'
  });

  await expectsRejection(async () => {
    const m = new SourceTextModule('');
    await m.evaluate(false);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "options" argument must be of type Object. ' +
             'Received type boolean'
  });

  await expectsRejection(async () => {
    const m = await createEmptyLinkedModule();
    await m.evaluate();
  }, {
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must be one of instantiated, evaluated, and errored'
  });

  common.expectsError(() => {
    const m = new SourceTextModule('');
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
    const m = new SourceTextModule('');
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
    const m = new SourceTextModule('import "foo";');
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
    const foo = new SourceTextModule('', { context: c });
    await foo.link(common.mustNotCall());
    const bar = new SourceTextModule('import "foo";');
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
    const erroredModule = new SourceTextModule('import "foo";');
    try {
      await erroredModule.link(common.mustCall(() => ({})));
    } catch {
      // ignored
    } finally {
      assert.strictEqual(erroredModule.linkingStatus, 'errored');
    }

    const rootModule = new SourceTextModule('import "errored";');
    await rootModule.link(common.mustCall(() => erroredModule));
  }, {
    code: 'ERR_VM_MODULE_LINKING_ERRORED'
  });
}

common.expectsError(() => {
  new SourceTextModule('', {
    importModuleDynamically: 'hucairz'
  });
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "options.importModuleDynamically"' +
    ' property must be of type function. Received type string'
});

// Check the JavaScript engine deals with exceptions correctly
async function checkExecution() {
  await (async () => {
    const m = new SourceTextModule('import { nonexistent } from "module";');
    await m.link(common.mustCall(() => new SourceTextModule('')));

    // There is no code for this exception since it is thrown by the JavaScript
    // engine.
    assert.throws(() => {
      m.instantiate();
    }, SyntaxError);
  })();

  await (async () => {
    const m = new SourceTextModule('throw new Error();');
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
