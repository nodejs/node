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
  let breakOnSigint = options && options.breakOnSigint;
  let runInThisContext = () => realRunInThisContext.call(this, options);

  if (breakOnSigint) return sigintHandlersWrap(runInThisContext);

  return runInThisContext();
};

Script.prototype.runInContext = function(contextifiedSandbox, options) {
  let breakOnSigint = options && options.breakOnSigint;
  let runInContext = () => realRunInContext.call(this, contextifiedSandbox, options);

  if (breakOnSigint) return sigintHandlersWrap(runInContext);

  return runInContext();
};

Script.prototype.runInNewContext = function(sandbox, options) {
  let context = exports.createContext(sandbox);
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
  let script = new Script(code, options);
  return script.runInContext(contextifiedSandbox, options);
};

exports.runInNewContext = function(code, sandbox, options) {
  let script = new Script(code, options);
  return script.runInNewContext(sandbox, options);
};

exports.runInThisContext = function(code, options) {
  let script = new Script(code, options);
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
