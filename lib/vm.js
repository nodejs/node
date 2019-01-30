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
  ContextifyScript,
  makeContext,
  isContext: _isContext,
  compileFunction: _compileFunction
} = internalBinding('contextify');
const { callbackMap } = internalBinding('module_wrap');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_VM_MODULE_NOT_MODULE,
} = require('internal/errors').codes;
const { isModuleNamespaceObject, isArrayBufferView } = require('util').types;
const {
  validateInt32,
  validateUint32,
  validateString
} = require('internal/validators');
const { kVmBreakFirstLineSymbol } = require('internal/util');
const kParsingContext = Symbol('script parsing context');

const ArrayForEach = Function.call.bind(Array.prototype.forEach);
const ArrayIsArray = Array.isArray;

class Script extends ContextifyScript {
  constructor(code, options = {}) {
    code = `${code}`;
    if (typeof options === 'string') {
      options = { filename: options };
    } else if (typeof options !== 'object' || options === null) {
      throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
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
    if (typeof produceCachedData !== 'boolean') {
      throw new ERR_INVALID_ARG_TYPE('options.produceCachedData', 'boolean',
                                     produceCachedData);
    }

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
      if (typeof importModuleDynamically !== 'function') {
        throw new ERR_INVALID_ARG_TYPE('options.importModuleDynamically',
                                       'function',
                                       importModuleDynamically);
      }
      const { wrapMap, linkingStatusMap } =
        require('internal/vm/source_text_module');
      callbackMap.set(this, { importModuleDynamically: async (...args) => {
        const m = await importModuleDynamically(...args);
        if (isModuleNamespaceObject(m)) {
          return m;
        }
        if (!m || !wrapMap.has(m))
          throw new ERR_VM_MODULE_NOT_MODULE();
        const childLinkingStatus = linkingStatusMap.get(m);
        if (childLinkingStatus === 'errored')
          throw m.error;
        return m.namespace;
      } });
    }
  }

  runInThisContext(options) {
    const { breakOnSigint, args } = getRunInContextArgs(options);
    if (breakOnSigint && process.listenerCount('SIGINT') > 0) {
      return sigintHandlersWrap(super.runInThisContext, this, args);
    } else {
      return super.runInThisContext(...args);
    }
  }

  runInContext(contextifiedSandbox, options) {
    validateContext(contextifiedSandbox);
    const { breakOnSigint, args } = getRunInContextArgs(options);
    if (breakOnSigint && process.listenerCount('SIGINT') > 0) {
      return sigintHandlersWrap(super.runInContext, this,
                                [contextifiedSandbox, ...args]);
    } else {
      return super.runInContext(contextifiedSandbox, ...args);
    }
  }

  runInNewContext(sandbox, options) {
    const context = createContext(sandbox, getContextOptions(options));
    return this.runInContext(context, options);
  }
}

function validateContext(sandbox) {
  if (typeof sandbox !== 'object' || sandbox === null) {
    throw new ERR_INVALID_ARG_TYPE('contextifiedSandbox', 'Object', sandbox);
  }
  if (!_isContext(sandbox)) {
    throw new ERR_INVALID_ARG_TYPE('contextifiedSandbox', 'vm.Context',
                                   sandbox);
  }
}

function validateBool(prop, propName) {
  if (prop !== undefined && typeof prop !== 'boolean')
    throw new ERR_INVALID_ARG_TYPE(propName, 'boolean', prop);
}

function validateObject(prop, propName) {
  if (prop !== undefined && (typeof prop !== 'object' || prop === null))
    throw new ERR_INVALID_ARG_TYPE(propName, 'Object', prop);
}

function getRunInContextArgs(options = {}) {
  if (typeof options !== 'object' || options === null) {
    throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
  }

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

  if (typeof displayErrors !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE('options.displayErrors', 'boolean',
                                   displayErrors);
  }
  if (typeof breakOnSigint !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE('options.breakOnSigint', 'boolean',
                                   breakOnSigint);
  }

  return {
    breakOnSigint,
    args: [timeout, displayErrors, breakOnSigint, breakFirstLine]
  };
}

function getContextOptions(options) {
  if (options) {
    validateObject(options.contextCodeGeneration,
                   'options.contextCodeGeneration');
    const contextOptions = {
      name: options.contextName,
      origin: options.contextOrigin,
      codeGeneration: typeof options.contextCodeGeneration === 'object' ? {
        strings: options.contextCodeGeneration.strings,
        wasm: options.contextCodeGeneration.wasm,
      } : undefined,
    };
    if (contextOptions.name !== undefined)
      validateString(contextOptions.name, 'options.contextName');
    if (contextOptions.origin !== undefined)
      validateString(contextOptions.origin, 'options.contextOrigin');
    if (contextOptions.codeGeneration) {
      validateBool(contextOptions.codeGeneration.strings,
                   'options.contextCodeGeneration.strings');
      validateBool(contextOptions.codeGeneration.wasm,
                   'options.contextCodeGeneration.wasm');
    }
    return contextOptions;
  }
  return {};
}

function isContext(sandbox) {
  if (typeof sandbox !== 'object' || sandbox === null) {
    throw new ERR_INVALID_ARG_TYPE('sandbox', 'Object', sandbox);
  }
  return _isContext(sandbox);
}

let defaultContextNameIndex = 1;
function createContext(sandbox = {}, options = {}) {
  if (isContext(sandbox)) {
    return sandbox;
  }

  if (typeof options !== 'object' || options === null) {
    throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
  }

  const {
    name = `VM Context ${defaultContextNameIndex++}`,
    origin,
    codeGeneration
  } = options;

  validateString(name, 'options.name');
  if (origin !== undefined)
    validateString(origin, 'options.origin');
  validateObject(codeGeneration, 'options.codeGeneration');

  let strings = true;
  let wasm = true;
  if (codeGeneration !== undefined) {
    ({ strings = true, wasm = true } = codeGeneration);
    validateBool(strings, 'options.codeGeneration.strings');
    validateBool(wasm, 'options.codeGeneration.wasm');
  }

  makeContext(sandbox, name, origin, strings, wasm);
  return sandbox;
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
    return fn.apply(thisArg, argsArray);
  } finally {
    // Add using the public methods so that the `newListener` handler of
    // process can re-attach the listeners.
    for (const listener of sigintListeners) {
      process.addListener('SIGINT', listener);
    }
  }
}

function runInContext(code, contextifiedSandbox, options) {
  validateContext(contextifiedSandbox);
  if (typeof options === 'string') {
    options = {
      filename: options,
      [kParsingContext]: contextifiedSandbox
    };
  } else {
    options = { ...options, [kParsingContext]: contextifiedSandbox };
  }
  return createScript(code, options)
    .runInContext(contextifiedSandbox, options);
}

function runInNewContext(code, sandbox, options) {
  if (typeof options === 'string') {
    options = { filename: options };
  }
  sandbox = createContext(sandbox, getContextOptions(options));
  options = { ...options, [kParsingContext]: sandbox };
  return createScript(code, options).runInNewContext(sandbox, options);
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
    if (!ArrayIsArray(params)) {
      throw new ERR_INVALID_ARG_TYPE('params', 'Array', params);
    }
    ArrayForEach(params, (param, i) => validateString(param, `params[${i}]`));
  }

  const {
    filename = '',
    columnOffset = 0,
    lineOffset = 0,
    cachedData = undefined,
    produceCachedData = false,
    parsingContext = undefined,
    contextExtensions = [],
  } = options;

  validateString(filename, 'options.filename');
  validateUint32(columnOffset, 'options.columnOffset');
  validateUint32(lineOffset, 'options.lineOffset');
  if (cachedData !== undefined && !isArrayBufferView(cachedData)) {
    throw new ERR_INVALID_ARG_TYPE(
      'options.cachedData',
      ['Buffer', 'TypedArray', 'DataView'],
      cachedData
    );
  }
  if (typeof produceCachedData !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.produceCachedData',
      'boolean',
      produceCachedData
    );
  }
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
  if (!ArrayIsArray(contextExtensions)) {
    throw new ERR_INVALID_ARG_TYPE(
      'options.contextExtensions',
      'Array',
      contextExtensions
    );
  }
  ArrayForEach(contextExtensions, (extension, i) => {
    if (typeof extension !== 'object') {
      throw new ERR_INVALID_ARG_TYPE(
        `options.contextExtensions[${i}]`,
        'object',
        extension
      );
    }
  });

  return _compileFunction(
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
};

if (require('internal/options').getOptionValue('--experimental-vm-modules')) {
  const { SourceTextModule } = require('internal/vm/source_text_module');
  module.exports.SourceTextModule = SourceTextModule;
}
