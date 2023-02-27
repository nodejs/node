'use strict';
const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');

common.skipIfDumbTerminal();

const readline = require('readline');
const rli = new readline.Interface({
  terminal: true,
  input: new ArrayStream(),
});

let recursionDepth = 0;

rli.on('line', function onLine() {
  // Abort in case of infinite loop
  if (recursionDepth > 2) {
    return;
  }
  recursionDepth++;
  // Write something recursively to readline
  rli.write('foo');
});

// Minimal reproduction for #46731
const testInput = ' \n}\n';
rli.write(testInput);

assert.strictEqual(recursionDepth, testInput.match(/\n/g).length);
