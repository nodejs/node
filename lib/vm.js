// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const {
  ArrayPrototypeForEach,
  ArrayPrototypeUnshift,
  Symbol,
  PromiseReject,
  ReflectApply,
} = primordials;

const {
  ContextifyScript,
  MicrotaskQueue,
  makeContext,
  isContext: _isContext,
  constants,
  compileFunction: _compileFunction,
  measureMemory: _measureMemory,
} = internalBinding('contextify');
const {
  ERR_CONTEXT_NOT_INITIALIZED,
  ERR_INVALID_ARG_TYPE,
} = require('internal/errors').codes;
const {
  isArrayBufferView,
} = require('internal/util/types');
const {
  validateArray,
  validateBoolean,
  validateBuffer,
  validateFunction,
  validateInt32,
  validateObject,
  validateOneOf,
  validateString,
  validateUint32,
} = require('internal/validators');
const {
  kVmBreakFirstLineSymbol,
  emitExperimentalWarning,
} = require('internal/util');
const kParsingContext = Symbol('script parsing context');

class Script extends ContextifyScript {
  constructor(code, options = {}) {
    code = `${code}`;
    if (typeof options === 'string') {
      options = { filename: options };
    } else {
      validateObject(options, 'options');
    }

    const {
      filename = 'evalmachine.<anonymous>',
      lineOffset = 0,
      columnOffset = 0,
      cachedData,
      produceCachedData = false,
      importModuleDynamically,
      [kParsingContext]: parsingContext,
    } = options;

    validateString(filename, 'options.filename');
    validateInt32(lineOffset, 'options.lineOffset');
    validateInt32(columnOffset, 'options.columnOffset');
    if (cachedData !== undefined && !isArrayBufferView(cachedData)) {
      throw new ERR_INVALID_ARG_TYPE(
        'options.cachedData',
        ['Buffer', 'TypedArray', 'DataView'],
        cachedData
      );
    }
    validateBoolean(produceCachedData, 'options.produceCachedData');

    // Calling `ReThrow()` on a native TryCatch does not generate a new
    // abort-on-uncaught-exception check. A dummy try/catch in JS land
    // protects against that.
    try { // eslint-disable-line no-useless-catch
      super(code,
            filename,
            lineOffset,
            columnOffset,
            cachedData,
            produceCachedData,
            parsingContext);
    } catch (e) {
      throw e; /* node-do-not-add-exception-line */
    }

    if (importModuleDynamically !== undefined) {
      validateFunction(importModuleDynamically,
                       'options.importModuleDynamically');
      const { importModuleDynamicallyWrap } =
        require('internal/vm/module');
      const { callbackMap } = internalBinding('module_wrap');
      callbackMap.set(this, {
        importModuleDynamically:
          importModuleDynamicallyWrap(importModuleDynamically),
      });
    }
  }

  runInThisContext(options) {
    const { breakOnSigint, args } = getRunInContextArgs(options);
    if (breakOnSigint && process.listenerCount('SIGINT') > 0) {
      return sigintHandlersWrap(super.runInThisContext, this, args);
    }
    return ReflectApply(super.runInThisContext, this, args);
  }

  runInContext(contextifiedObject, options) {
    validateContext(contextifiedObject);
    const { breakOnSigint, args } = getRunInContextArgs(options);
    ArrayPrototypeUnshift(args, contextifiedObject);
    if (breakOnSigint && process.listenerCount('SIGINT') > 0) {
      return sigintHandlersWrap(super.runInContext, this, args);
    }
    return ReflectApply(super.runInContext, this, args);
  }

  runInNewContext(contextObject, options) {
    const context = createContext(contextObject, getContextOptions(options));
    return this.runInContext(context, options);
  }
}

function validateContext(contextifiedObject) {
  if (!isContext(contextifiedObject)) {
    throw new ERR_INVALID_ARG_TYPE('contextifiedObject', 'vm.Context',
                                   contextifiedObject);
  }
}

function getRunInContextArgs(options = {}) {
  validateObject(options, 'options');

  let timeout = options.timeout;
  if (timeout === undefined) {
    timeout = -1;
  } else {
    validateUint32(timeout, 'options.timeout', true);
  }

  const {
    displayErrors = true,
    breakOnSigint = false,
    [kVmBreakFirstLineSymbol]: breakFirstLine = false,
  } = options;

  validateBoolean(displayErrors, 'options.displayErrors');
  validateBoolean(breakOnSigint, 'options.breakOnSigint');

  return {
    breakOnSigint,
    args: [timeout, displayErrors, breakOnSigint, breakFirstLine]
  };
}

function getContextOptions(options) {
  if (!options)
    return {};
  const contextOptions = {
    name: options.contextName,
    origin: options.contextOrigin,
    codeGeneration: undefined,
    microtaskMode: options.microtaskMode,
  };
  if (contextOptions.name !== undefined)
    validateString(contextOptions.name, 'options.contextName');
  if (contextOptions.origin !== undefined)
    validateString(contextOptions.origin, 'options.contextOrigin');
  if (options.contextCodeGeneration !== undefined) {
    validateObject(options.contextCodeGeneration,
                   'options.contextCodeGeneration');
    const { strings, wasm } = options.contextCodeGeneration;
    if (strings !== undefined)
      validateBoolean(strings, 'options.contextCodeGeneration.strings');
    if (wasm !== undefined)
      validateBoolean(wasm, 'options.contextCodeGeneration.wasm');
    contextOptions.codeGeneration = { strings, wasm };
  }
  if (options.microtaskMode !== undefined)
    validateString(options.microtaskMode, 'options.microtaskMode');
  return contextOptions;
}

function isContext(object) {
  validateObject(object, 'object', { allowArray: true });

  return _isContext(object);
}

let defaultContextNameIndex = 1;
function createContext(contextObject = {}, options = {}) {
  if (isContext(contextObject)) {
    return contextObject;
  }

  validateObject(options, 'options');

  const {
    name = `VM Context ${defaultContextNameIndex++}`,
    origin,
    codeGeneration,
    microtaskMode
  } = options;

  validateString(name, 'options.name');
  if (origin !== undefined)
    validateString(origin, 'options.origin');
  if (codeGeneration !== undefined)
    validateObject(codeGeneration, 'options.codeGeneration');

  let strings = true;
  let wasm = true;
  if (codeGeneration !== undefined) {
    ({ strings = true, wasm = true } = codeGeneration);
    validateBoolean(strings, 'options.codeGeneration.strings');
    validateBoolean(wasm, 'options.codeGeneration.wasm');
  }

  let microtaskQueue = null;
  if (microtaskMode !== undefined) {
    validateOneOf(microtaskMode, 'options.microtaskMode',
                  ['afterEvaluate', undefined]);

    if (microtaskMode === 'afterEvaluate')
      microtaskQueue = new MicrotaskQueue();
  }

  makeContext(contextObject, name, origin, strings, wasm, microtaskQueue);
  return contextObject;
}

function createScript(code, options) {
  return new Script(code, options);
}

// Remove all SIGINT listeners and re-attach them after the wrapped function
// has executed, so that caught SIGINT are handled by the listeners again.
function sigintHandlersWrap(fn, thisArg, argsArray) {
  const sigintListeners = process.rawListeners('SIGINT');

  process.removeAllListeners('SIGINT');

  try {
    return ReflectApply(fn, thisArg, argsArray);
  } finally {
    // Add using the public methods so that the `newListener` handler of
    // process can re-attach the listeners.
    ArrayPrototypeForEach(sigintListeners, (listener) => {
      process.addListener('SIGINT', listener);
    });
  }
}

function runInContext(code, contextifiedObject, options) {
  validateContext(contextifiedObject);
  if (typeof options === 'string') {
    options = {
      filename: options,
      [kParsingContext]: contextifiedObject
    };
  } else {
    options = { ...options, [kParsingContext]: contextifiedObject };
  }
  return createScript(code, options)
    .runInContext(contextifiedObject, options);
}

function runInNewContext(code, contextObject, options) {
  if (typeof options === 'string') {
    options = { filename: options };
  }
  contextObject = createContext(contextObject, getContextOptions(options));
  options = { ...options, [kParsingContext]: contextObject };
  return createScript(code, options).runInNewContext(contextObject, options);
}

function runInThisContext(code, options) {
  if (typeof options === 'string') {
    options = { filename: options };
  }
  return createScript(code, options).runInThisContext(options);
}

function compileFunction(code, params, options = {}) {
  validateString(code, 'code');
  if (params !== undefined) {
    validateArray(params, 'params');
    ArrayPrototypeForEach(params,
                          (param, i) => validateString(param, `params[${i}]`));
  }

  const {
    filename = '',
    columnOffset = 0,
    lineOffset = 0,
    cachedData = undefined,
    produceCachedData = false,
    parsingContext = undefined,
    contextExtensions = [],
    importModuleDynamically,
  } = options;

  validateString(filename, 'options.filename');
  validateUint32(columnOffset, 'options.columnOffset');
  validateUint32(lineOffset, 'options.lineOffset');
  if (cachedData !== undefined)
    validateBuffer(cachedData, 'options.cachedData');
  validateBoolean(produceCachedData, 'options.produceCachedData');
  if (parsingContext !== undefined) {
    if (
      typeof parsingContext !== 'object' ||
      parsingContext === null ||
      !isContext(parsingContext)
    ) {
      throw new ERR_INVALID_ARG_TYPE(
        'options.parsingContext',
        'Context',
        parsingContext
      );
    }
  }
  validateArray(contextExtensions, 'options.contextExtensions');
  ArrayPrototypeForEach(contextExtensions, (extension, i) => {
    const name = `options.contextExtensions[${i}]`;
    validateObject(extension, name, { nullable: true });
  });

  const result = _compileFunction(
    code,
    filename,
    lineOffset,
    columnOffset,
    cachedData,
    produceCachedData,
    parsingContext,
    contextExtensions,
    params
  );

  if (produceCachedData) {
    result.function.cachedDataProduced = result.cachedDataProduced;
  }

  if (result.cachedData) {
    result.function.cachedData = result.cachedData;
  }

  if (importModuleDynamically !== undefined) {
    validateFunction(importModuleDynamically,
                     'options.importModuleDynamically');
    const { importModuleDynamicallyWrap } =
      require('internal/vm/module');
    const { callbackMap } = internalBinding('module_wrap');
    const wrapped = importModuleDynamicallyWrap(importModuleDynamically);
    const func = result.function;
    callbackMap.set(result.cacheKey, {
      importModuleDynamically: (s, _k) => wrapped(s, func),
    });
  }

  return result.function;
}

const measureMemoryModes = {
  summary: constants.measureMemory.mode.SUMMARY,
  detailed: constants.measureMemory.mode.DETAILED,
};

const measureMemoryExecutions = {
  default: constants.measureMemory.execution.DEFAULT,
  eager: constants.measureMemory.execution.EAGER,
};

function measureMemory(options = {}) {
  emitExperimentalWarning('vm.measureMemory');
  validateObject(options, 'options');
  const { mode = 'summary', execution = 'default' } = options;
  validateOneOf(mode, 'options.mode', ['summary', 'detailed']);
  validateOneOf(execution, 'options.execution', ['default', 'eager']);
  const result = _measureMemory(measureMemoryModes[mode],
                                measureMemoryExecutions[execution]);
  if (result === undefined) {
    return PromiseReject(new ERR_CONTEXT_NOT_INITIALIZED());
  }
  return result;
}

module.exports = {
  Script,
  createContext,
  createScript,
  runInContext,
  runInNewContext,
  runInThisContext,
  isContext,
  compileFunction,
  measureMemory,
};

// The vm module is patched to include vm.Module, vm.SourceTextModule
// and vm.SyntheticModule in the pre-execution phase when
// --experimental-vm-modules is on.
