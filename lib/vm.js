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

var binding = process.binding('contextify');
var Script = binding.ContextifyScript;
var util = require('util');

// The binding provides a few useful primitives:
// - ContextifyScript(code, [filename]), with methods:
//   - runInThisContext()
//   - runInContext(sandbox, [timeout])
// - makeContext(sandbox)
// From this we build the entire documented API.

Script.prototype.runInNewContext = function(initSandbox, timeout, disp) {
  var context = exports.createContext(initSandbox);
  return this.runInContext(context, timeout, disp);
};

exports.Script = Script;

exports.createScript = function(code, filename, disp) {
  return new Script(code, filename, disp);
};

exports.createContext = function(initSandbox) {
  if (util.isUndefined(initSandbox)) {
    initSandbox = {};
  }

  binding.makeContext(initSandbox);

  return initSandbox;
};

exports.runInContext = function(code, sandbox, filename, timeout, disp) {
  var script = exports.createScript(code, filename, disp);
  return script.runInContext(sandbox, timeout, disp);
};

exports.runInNewContext = function(code, sandbox, filename, timeout, disp) {
  var script = exports.createScript(code, filename, disp);
  return script.runInNewContext(sandbox, timeout, disp);
};

exports.runInThisContext = function(code, filename, timeout, disp) {
  var script = exports.createScript(code, filename, disp);
  return script.runInThisContext(timeout, disp);
};
