const readline = require('readline');

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
  terminal: true
});

console.log('Testing cursor movement in multi-line input...');
console.log('Type a multi-line function and use Up/Down arrows to navigate.');
console.log('Press Ctrl+C to exit.');

rl.prompt();