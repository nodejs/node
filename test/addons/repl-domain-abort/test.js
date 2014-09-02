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
var repl = require('repl');
var stream = require('stream');
var buildType = process.config.target_defaults.default_configuration;
var buildPath = __dirname + '/build/' + buildType + '/binding';
var cb_ran = false;

process.on('exit', function() {
  assert(cb_ran);
  console.log('ok');
});

var lines = [
  // This line shouldn't cause an assertion error.
  'require(\'' + buildPath + '\')' +
  // Log output to double check callback ran.
  '.method(function() { console.log(\'cb_ran\'); });',
];

var dInput = new stream.Readable();
var dOutput = new stream.Writable();

dInput._read = function _read(size) {
  while (lines.length > 0 && this.push(lines.shift()));
  if (lines.length === 0)
    this.push(null);
};

dOutput._write = function _write(chunk, encoding, cb) {
  if (chunk.toString().indexOf('cb_ran') === 0)
    cb_ran = true;
  cb();
};

var options = {
  input: dInput,
  output: dOutput,
  terminal: false,
  ignoreUndefined: true
};

// Run commands from fake REPL.
var dummy = repl.start(options);
