'use strict';

const binding = process.binding('contextify');
const Script = binding.ContextifyScript;

// The binding provides a few useful primitives:
// - ContextifyScript(code, { filename = "evalmachine.anonymous",
//                            displayErrors = true } = {})
//   with methods:
//   - runInThisContext({ displayErrors = true } = {})
//   - runInContext(sandbox, { displayErrors = true, timeout = undefined } = {})
// - makeContext(sandbox)
// - isContext(sandbox)
// From this we build the entire documented API.

const realRunInThisContext = Script.prototype.runInThisContext;
const realRunInContext = Script.prototype.runInContext;

Script.prototype.runInThisContext = function(options) {
  if (options && options.breakOnSigint) {
    return sigintHandlersWrap(() => {
      return realRunInThisContext.call(this, options);
    });
  } else {
    return realRunInThisContext.call(this, options);
  }
};

Script.prototype.runInContext = function(contextifiedSandbox, options) {
  if (options && options.breakOnSigint) {
    return sigintHandlersWrap(() => {
      return realRunInContext.call(this, contextifiedSandbox, options);
    });
  } else {
    return realRunInContext.call(this, contextifiedSandbox, options);
  }
};

Script.prototype.runInNewContext = function(sandbox, options) {
  var context = exports.createContext(sandbox);
  return this.runInContext(context, options);
};

exports.Script = Script;

exports.createScript = function(code, options) {
  return new Script(code, options);
};

exports.createContext = function(sandbox) {
  if (sandbox === undefined) {
    sandbox = {};
  } else if (binding.isContext(sandbox)) {
    return sandbox;
  }

  binding.makeContext(sandbox);
  return sandbox;
};

exports.runInDebugContext = function(code) {
  return binding.runInDebugContext(code);
};

exports.runInContext = function(code, contextifiedSandbox, options) {
  var script = new Script(code, options);
  return script.runInContext(contextifiedSandbox, options);
};

exports.runInNewContext = function(code, sandbox, options) {
  var script = new Script(code, options);
  return script.runInNewContext(sandbox, options);
};

exports.runInThisContext = function(code, options) {
  var script = new Script(code, options);
  return script.runInThisContext(options);
};

exports.isContext = binding.isContext;

// Remove all SIGINT listeners and re-attach them after the wrapped function
// has executed, so that caught SIGINT are handled by the listeners again.
function sigintHandlersWrap(fn) {
  // Using the internal list here to make sure `.once()` wrappers are used,
  // not the original ones.
  let sigintListeners = process._events.SIGINT;
  if (!Array.isArray(sigintListeners))
    sigintListeners = sigintListeners ? [sigintListeners] : [];
  else
    sigintListeners = sigintListeners.slice();

  process.removeAllListeners('SIGINT');

  try {
    return fn();
  } finally {
    // Add using the public methods so that the `newListener` handler of
    // process can re-attach the listeners.
    for (const listener of sigintListeners) {
      process.addListener('SIGINT', listener);
    }
  }
}
