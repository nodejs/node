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
  kParsingContext,
  makeContext,
  isContext: _isContext,
} = process.binding('contextify');

const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;
const { isUint8Array } = require('internal/util/types');
const { validateInt32, validateUint32 } = require('internal/validators');

class Script extends ContextifyScript {
  constructor(code, options = {}) {
    code = `${code}`;
    if (typeof options === 'string') {
      options = { filename: options };
    }
    if (typeof options !== 'object' || options === null) {
      throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
    }

    const {
      filename = 'evalmachine.<anonymous>',
      lineOffset = 0,
      columnOffset = 0,
      cachedData,
      produceCachedData = false,
      [kParsingContext]: parsingContext
    } = options;

    if (typeof filename !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('options.filename', 'string', filename);
    }
    validateInt32(lineOffset, 'options.lineOffset');
    validateInt32(columnOffset, 'options.columnOffset');
    if (cachedData !== undefined && !isUint8Array(cachedData)) {
      throw new ERR_INVALID_ARG_TYPE('options.cachedData',
                                     ['Buffer', 'Uint8Array'], cachedData);
    }
    if (typeof produceCachedData !== 'boolean') {
      throw new ERR_INVALID_ARG_TYPE('options.produceCachedData', 'boolean',
                                     produceCachedData);
    }

    // Calling `ReThrow()` on a native TryCatch does not generate a new
    // abort-on-uncaught-exception check. A dummy try/catch in JS land
    // protects against that.
    try {
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

function validateString(prop, propName) {
  if (prop !== undefined && typeof prop !== 'string')
    throw new ERR_INVALID_ARG_TYPE(propName, 'string', prop);
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
    breakOnSigint = false
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
    args: [timeout, displayErrors, breakOnSigint]
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
    validateString(contextOptions.name, 'options.contextName');
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

  if (typeof name !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('options.name', 'string', options.name);
  }
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
    options = Object.assign({}, options, {
      [kParsingContext]: contextifiedSandbox
    });
  }
  return createScript(code, options)
    .runInContext(contextifiedSandbox, options);
}

function runInNewContext(code, sandbox, options) {
  if (typeof options === 'string') {
    options = { filename: options };
  }
  sandbox = createContext(sandbox, getContextOptions(options));
  options = Object.assign({}, options, {
    [kParsingContext]: sandbox
  });
  return createScript(code, options).runInNewContext(sandbox, options);
}

function runInThisContext(code, options) {
  if (typeof options === 'string') {
    options = { filename: options };
  }
  return createScript(code, options).runInThisContext(options);
}

module.exports = {
  Script,
  createContext,
  createScript,
  runInContext,
  runInNewContext,
  runInThisContext,
  isContext,
};

if (process.binding('config').experimentalVMModules)
  module.exports.Module = require('internal/vm/module').Module;
