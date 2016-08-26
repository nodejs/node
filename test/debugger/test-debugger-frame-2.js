'use strict';
require('../common');
const repl = require('./helper-debugger-repl.js');

repl.startDebugger('frames.js');

const addTest = repl.addTest;

// Continue
addTest('c', [
  /break in .*:3/,
  /1/, /2/, /3/, /4/, /5/
]);

// Select frame 1
addTest('frame(1)', [
  /Frame selected: 1/
]);

// Select frame 0
addTest('frame(0)', [
  /Frame selected: 0/
]);

// Repl
addTest('repl', [
  /Ctrl/
]);

// Value of a is from selected frame
addTest('a', [
  /bbb/
]);
