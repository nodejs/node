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

addTest('arr = [{foo: "bar"}]', [
  /\[ \{ foo: 'bar' \} \]/
]);
