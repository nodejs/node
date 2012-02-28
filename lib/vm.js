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

var binding = process.binding('evals');

module.exports = Script;
Script.Script = Script;

function Script(code, ctx, filename) {
  if (!(this instanceof Script)) {
    return new Script(code, ctx, filename);
  }

  var ns = new binding.NodeScript(code, ctx, filename);

  // bind all methods to this Script object
  Object.keys(binding.NodeScript.prototype).forEach(function(f) {
    if (typeof binding.NodeScript.prototype[f] === 'function') {
      this[f] = function() {
        if (!(this instanceof Script)) {
          throw new TypeError('invalid call to '+f);
        }
        return ns[f].apply(ns, arguments);
      };
    }
  }, this);
};

Script.createScript = function(code, ctx, name) {
  return new Script(code, ctx, name);
};

Script.createContext = binding.NodeScript.createContext;
Script.runInContext = binding.NodeScript.runInContext;
Script.runInThisContext = binding.NodeScript.runInThisContext;
Script.runInNewContext = binding.NodeScript.runInNewContext;
