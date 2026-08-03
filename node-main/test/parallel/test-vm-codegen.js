'use strict';

require('../common');
const assert = require('assert');

const { createContext, runInContext, runInNewContext } = require('vm');

const WASM_BYTES = Buffer.from(
  [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);

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
  assert.throws(() => {
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
  assert.throws(() => {
    runInContext('new WebAssembly.Module(WASM_BYTES)', ctx);
  }, CompileError);
}

assert.throws(() => {
  runInNewContext('eval("x")', {}, {
    contextCodeGeneration: {
      strings: false,
    },
  });
}, {
  name: 'EvalError'
});

assert.throws(() => {
  runInNewContext('new WebAssembly.Module(WASM_BYTES)', { WASM_BYTES }, {
    contextCodeGeneration: {
      wasm: false,
    },
  });
}, {
  name: 'CompileError'
});

assert.throws(() => {
  createContext({}, {
    codeGeneration: {
      strings: 0,
    },
  });
}, {
  code: 'ERR_INVALID_ARG_TYPE',
});

assert.throws(() => {
  runInNewContext('eval("x")', {}, {
    contextCodeGeneration: {
      wasm: 1,
    },
  });
}, {
  code: 'ERR_INVALID_ARG_TYPE'
});

assert.throws(() => {
  createContext({}, {
    codeGeneration: 1,
  });
}, {
  code: 'ERR_INVALID_ARG_TYPE',
});

assert.throws(() => {
  createContext({}, {
    codeGeneration: null,
  });
}, {
  code: 'ERR_INVALID_ARG_TYPE',
});
