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

// List lines for frame 0
addTest('list(1)', [
  /2/, /3/, /4/
]);

// Invalid frame number
addTest('frame(10)', [
  /Invalid frame number/
]);

// Select frame 1 (using f)
addTest('f(1)', [
  /Frame selected: 1/
]);

// List lines for frame 1
addTest('list(1)', [
  /4/, /5/, /6/
]);

// Repl
addTest('repl', [
  /Ctrl/
]);

// Value of a is from selected frame
addTest('a', [
  /aaa/
]);

// For the assertion that 'a' returns the right results when we move
// back to frame 0, see test-debugger-frame-2.js
