'use strict';
require('../common');

process.env.TERM = 'dumb';

const readline = require('readline');

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
});

rl.write('text');
rl.write(null, { ctrl: true, name: 'u' });
rl.write(null, { name: 'return' });
rl.write('text');
rl.write(null, { name: 'backspace' });
rl.write(null, { name: 'escape' });
rl.write(null, { name: 'enter' });
rl.write('text');
rl.write(null, { ctrl: true, name: 'c' });
