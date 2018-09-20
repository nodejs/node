'use strict';

const common = require('../common');
const assert = require('assert');

const { createContext, runInContext, runInNewContext } = require('vm');

const WASM_BYTES = Buffer.from(
  [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);


function expectsError(fn, type) {
  try {
    fn();
    assert.fail('expected fn to error');
  } catch (err) {
    if (typeof type === 'string')
      assert.strictEqual(err.name, type);
    else
      assert(err instanceof type);
  }
}

{
  const ctx = createContext({ WASM_BYTES });
  const test = 'eval(""); new WebAssembly.Module(WASM_BYTES);';
  runInContext(test, ctx);

  runInNewContext(test, { WASM_BYTES }, {
    contextCodeGeneration: undefined,
  });
}

{
  const ctx = createContext({}, {
    codeGeneration: {
      strings: false,
    },
  });

  const EvalError = runInContext('EvalError', ctx);
  expectsError(() => {
    runInContext('eval("x")', ctx);
  }, EvalError);
}

{
  const ctx = createContext({ WASM_BYTES }, {
    codeGeneration: {
      wasm: false,
    },
  });

  const CompileError = runInContext('WebAssembly.CompileError', ctx);
  expectsError(() => {
    runInContext('new WebAssembly.Module(WASM_BYTES)', ctx);
  }, CompileError);
}

expectsError(() => {
  runInNewContext('eval("x")', {}, {
    contextCodeGeneration: {
      strings: false,
    },
  });
}, 'EvalError');

expectsError(() => {
  runInNewContext('new WebAssembly.Module(WASM_BYTES)', { WASM_BYTES }, {
    contextCodeGeneration: {
      wasm: false,
    },
  });
}, 'CompileError');

common.expectsError(() => {
  createContext({}, {
    codeGeneration: {
      strings: 0,
    },
  });
}, {
  code: 'ERR_INVALID_ARG_TYPE',
});

common.expectsError(() => {
  runInNewContext('eval("x")', {}, {
    contextCodeGeneration: {
      wasm: 1,
    },
  });
}, {
  code: 'ERR_INVALID_ARG_TYPE'
});

common.expectsError(() => {
  createContext({}, {
    codeGeneration: 1,
  });
}, {
  code: 'ERR_INVALID_ARG_TYPE',
});

common.expectsError(() => {
  createContext({}, {
    codeGeneration: null,
  });
}, {
  code: 'ERR_INVALID_ARG_TYPE',
});
