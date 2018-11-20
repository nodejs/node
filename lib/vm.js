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
