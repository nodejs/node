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

var assert = require('assert');
var vm = require('vm');
var spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  var code = 'var j = 0;\n' +
      'for (var i = 0; i < 1000000; i++) j += add(i, i + 1);\n'
      'j;';

  var ctx = vm.createContext({
    add: function(x, y) {
      return x + y;
    }
  });

  vm.runInContext(code, ctx, { timeout: 1 });
} else {
  var proc = spawn(process.execPath, process.argv.slice(1).concat('child'));
  var err = '';
  proc.stderr.on('data', function(data) {
    err += data;
  });

  process.on('exit', function() {
    assert.ok(/Script execution timed out/.test(err));
  });
}
