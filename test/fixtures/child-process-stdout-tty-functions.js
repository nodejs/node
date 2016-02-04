'use strict';
const assert = require('assert');

const stdout = process.stdout;

try {
  stdout.cursorTo();
}
catch (e) {
  console.error(new Error(e));
}

try {
  stdout.moveCursor();
}
catch (e) {
  console.error(new Error(e));
}

try {
  stdout.clearLine();
}
catch (e) {
  console.error(new Error(e));
}

try {
  stdout.clearScreenDown();
}
catch (e) {
  console.error(new Error(e));
}

try {
  const windowSize = stdout.getWindowSize();
  assert.deepEqual(windowSize, [0, 0],
    'the window size should be [0,0]');
}
catch (e) {
  console.error(new Error(e));
}
