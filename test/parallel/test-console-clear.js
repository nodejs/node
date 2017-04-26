'use strict';

require('../common');
const assert = require('assert');

const stdoutWrite = process.stdout.write;

// The sequence for moving the cursor to 0,0 and clearing screen down
const check = '\u001b[1;1H\u001b[0J';

let buf = '';

// Fake TTY
process.stdout.isTTY = true;

process.stdout.write = (string) => buf += string;

console.clear();

process.stdout.write = stdoutWrite;

assert.strictEqual(buf, check);
