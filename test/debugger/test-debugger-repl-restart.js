'use strict';
require('../common');
var repl = require('./helper-debugger-repl.js');

repl.startDebugger('breakpoints.js');
var linesWithBreakpoint = [
  /1/, /2/, /3/, /4/, /5/, /\* 6/
];
// We slice here, because addTest will change the given array.
repl.addTest('sb(6)', linesWithBreakpoint.slice());

var initialLines = repl.initialLines.slice();
initialLines.splice(2, 0, /Restoring/, /Warning/);

// Restart the debugged script
repl.addTest('restart', [
  /terminated/,
].concat(initialLines));

repl.addTest('list(5)', linesWithBreakpoint);
repl.addTest('quit', []);
