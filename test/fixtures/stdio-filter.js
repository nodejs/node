const util = require('util');

const regexIn = process.argv[2];
const replacement = process.argv[3];
const re = new RegExp(regexIn, 'g');
const stdin = process.openStdin();

stdin.on('data', (data) => {
  data = data.toString();
  process.stdout.write(data.replace(re, replacement));
});
