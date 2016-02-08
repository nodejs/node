'use strict';
require('../common');
var repl = require('./helper-debugger-repl.js');

repl.startDebugger('breakpoints.js');

var addTest = repl.addTest;

// Next
addTest('n', [
  /break in .*:11/,
  /9/, /10/, /11/, /12/, /13/
]);

// Watch
addTest('watch("\'x\'")');

// Continue
addTest('c', [
  /break in .*:5/,
  /Watchers/,
  /0:\s+'x' = "x"/,
  /()/,
  /3/, /4/, /5/, /6/, /7/
]);

// Show watchers
addTest('watchers', [
  /0:\s+'x' = "x"/
]);

// Unwatch
addTest('unwatch("\'x\'")');

// Step out
addTest('o', [
  /break in .*:12/,
  /10/, /11/, /12/, /13/, /14/
]);

// Continue
addTest('c', [
  /break in .*:5/,
  /3/, /4/, /5/, /6/, /7/
]);

// Set breakpoint by function name
addTest('sb("setInterval()", "!(setInterval.flag++)")', [
  /1/, /2/, /3/, /4/, /5/, /6/, /7/, /8/, /9/, /10/
]);

// Continue
addTest('c', [
  /break in timers.js:\d+/,
  /\d/, /\d/, /\d/, /\d/, /\d/
]);

// Execute
addTest('exec process.title', [
  /node/
]);

// Execute
addTest('exec exec process.title', [
  /SyntaxError: Unexpected identifier/
]);

// REPL and process.env regression
addTest('repl', [
  /Ctrl/
]);

addTest('for (var i in process.env) delete process.env[i]', []);

addTest('process.env', [
  /\{\}/
]);

addTest('new Buffer([1, 10, 20])', [
  '<Buffer 01 0a 14>'
]);

addTest('var bigBuf = Buffer(200); for (var i=0; i < bb.length; ++i) bb[i] = i',
  []
);
addTest('bigBuf', [
  /28 29 2a 2b 2c 2d 2e 2f 30 31 \.\.\. >$/
]);
