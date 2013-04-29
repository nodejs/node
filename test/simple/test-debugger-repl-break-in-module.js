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

var repl = require('./helper-debugger-repl.js');

repl.startDebugger('break-in-module/main.js');

// -- SET BREAKPOINT --

// Set breakpoint by file name + line number where the file is not loaded yet
repl.addTest('sb("mod.js", 23)', [
  /Warning: script 'mod\.js' was not loaded yet\./,
  /1/, /2/, /3/, /4/, /5/, /6/
]);

// Check escaping of regex characters
repl.addTest('sb(")^$*+?}{|][(.js\\\\", 1)', [
  /Warning: script '[^']+' was not loaded yet\./,
  /1/, /2/, /3/, /4/, /5/, /6/
]);

// continue - the breakpoint should be triggered
repl.addTest('c', [
    /break in .*[\\\/]mod\.js:23/,
    /21/, /22/, /23/, /24/, /25/
]);

// -- RESTORE BREAKPOINT ON RESTART --

// Restart the application - breakpoint should be restored
repl.addTest('restart', [].concat(
  [
    /terminated/
  ],
  repl.handshakeLines,
  [
    /Restoring breakpoint mod.js:23/,
    /Warning: script 'mod\.js' was not loaded yet\./,
    /Restoring breakpoint \).*:\d+/,
    /Warning: script '\)[^']*' was not loaded yet\./
  ],
  repl.initialBreakLines));

// continue - the breakpoint should be triggered
repl.addTest('c', [
  /break in .*[\\\/]mod\.js:23/,
  /21/, /22/, /23/, /24/, /25/
]);

// -- CLEAR BREAKPOINT SET IN MODULE TO BE LOADED --

repl.addTest('cb("mod.js", 23)', [
  /18/, /./, /./, /./, /./, /./, /./, /./, /26/
]);

repl.addTest('c', [
  /break in .*[\\\/]main\.js:4/,
  /2/, /3/, /4/, /5/, /6/
]);

// -- (END) --
repl.addTest('quit', []);
