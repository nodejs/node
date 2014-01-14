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

process.env.NODE_FORCE_READLINE = 1;

var repl = require('./helper-debugger-repl.js');

repl.startDebugger('breakpoints.js');

var addTest = repl.addTest;

// next
addTest('n', [
  /debug>.*n/,
  /break in .*:11/,
  /9/, /10/, /11/, /12/, /13/
]);

// should repeat next
addTest('', [
  /debug>/,
  /break in .*:5/,
  /3/, /4/, /5/, /6/, /7/,
]);

// continue
addTest('c', [
  /debug>.*c/,
  /break in .*:12/,
  /10/, /11/, /12/, /13/, /14/
]);

// should repeat continue
addTest('', [
  /debug>/,
  /break in .*:5/,
  /3/, /4/, /5/, /6/, /7/,
]);

// should repeat continue
addTest('', [
  /debug>/,
  /break in .*:23/,
  /21/, /22/, /23/, /24/, /25/,
]);
