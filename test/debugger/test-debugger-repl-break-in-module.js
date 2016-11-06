'use strict';
require('../common');
var repl = require('./helper-debugger-repl.js');

repl.startDebugger('break-in-module/main.js');

// -- SET BREAKPOINT --

// Set breakpoint by file name + line number where the file is not loaded yet
repl.addTest('sb("mod.js", 2)', [
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
  /break in .*[\\/]mod\.js:2/,
  /1/, /2/, /3/, /4/
]);

// -- RESTORE BREAKPOINT ON RESTART --

// Restart the application - breakpoint should be restored
repl.addTest('restart', [].concat(
  [
    /terminated/
  ],
  repl.handshakeLines,
  [
    /Restoring breakpoint mod.js:2/,
    /Warning: script 'mod\.js' was not loaded yet\./,
    /Restoring breakpoint \).*:\d+/,
    /Warning: script '\)[^']*' was not loaded yet\./
  ],
  repl.initialBreakLines));

// continue - the breakpoint should be triggered
repl.addTest('c', [
  /break in .*[\\/]mod\.js:2/,
  /1/, /2/, /3/, /4/
]);

// -- CLEAR BREAKPOINT SET IN MODULE TO BE LOADED --

repl.addTest('cb("mod.js", 2)', [
  /1/, /2/, /3/, /4/, /5/
]);

repl.addTest('c', [
  /break in .*[\\/]main\.js:4/,
  /2/, /3/, /4/, /5/, /6/
]);

// -- (END) --
repl.addTest('quit', []);
