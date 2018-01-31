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
  isContext,
} = process.binding('contextify');

const errors = require('internal/errors');

// The binding provides a few useful primitives:
// - Script(code, { filename = "evalmachine.anonymous",
//                  displayErrors = true } = {})
//   with methods:
//   - runInThisContext({ displayErrors = true } = {})
//   - runInContext(sandbox, { displayErrors = true, timeout = undefined } = {})
// - makeContext(sandbox)
// - isContext(sandbox)
// From this we build the entire documented API.

class Script extends ContextifyScript {
  constructor(code, options) {
    // Calling `ReThrow()` on a native TryCatch does not generate a new
    // abort-on-uncaught-exception check. A dummy try/catch in JS land
    // protects against that.
    try {
      super(code, options);
    } catch (e) {
      throw e; /* node-do-not-add-exception-line */
    }
  }
}

const realRunInThisContext = Script.prototype.runInThisContext;
const realRunInContext = Script.prototype.runInContext;

Script.prototype.runInThisContext = function(options) {
  if (options && options.breakOnSigint && process.listenerCount('SIGINT') > 0) {
    return sigintHandlersWrap(realRunInThisContext, this, [options]);
  } else {
    return realRunInThisContext.call(this, options);
  }
};

Script.prototype.runInContext = function(contextifiedSandbox, options) {
  if (options && options.breakOnSigint && process.listenerCount('SIGINT') > 0) {
    return sigintHandlersWrap(realRunInContext, this,
                              [contextifiedSandbox, options]);
  } else {
    return realRunInContext.call(this, contextifiedSandbox, options);
  }
};

Script.prototype.runInNewContext = function(sandbox, options) {
  const context = createContext(sandbox, getContextOptions(options));
  return this.runInContext(context, options);
};

function getContextOptions(options) {
  const contextOptions = options ? {
    name: options.contextName,
    origin: options.contextOrigin
  } : {};
  if (contextOptions.name !== undefined &&
      typeof contextOptions.name !== 'string') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'options.contextName',
                               'string', contextOptions.name);
  }
  if (contextOptions.origin !== undefined &&
      typeof contextOptions.origin !== 'string') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'options.contextOrigin',
                               'string', contextOptions.origin);
  }
  return contextOptions;
}

let defaultContextNameIndex = 1;
function createContext(sandbox, options) {
  if (sandbox === undefined) {
    sandbox = {};
  } else if (isContext(sandbox)) {
    return sandbox;
  }

  if (options !== undefined) {
    if (typeof options !== 'object' || options === null) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'options',
                                 'object', options);
    }
    options = {
      name: options.name,
      origin: options.origin
    };
    if (options.name === undefined) {
      options.name = `VM Context ${defaultContextNameIndex++}`;
    } else if (typeof options.name !== 'string') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'options.name',
                                 'string', options.name);
    }
    if (options.origin !== undefined && typeof options.origin !== 'string') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'options.origin',
                                 'string', options.origin);
    }
  } else {
    options = {
      name: `VM Context ${defaultContextNameIndex++}`
    };
  }
  makeContext(sandbox, options);
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
  module.exports.Module = require('internal/vm/Module').Module;
