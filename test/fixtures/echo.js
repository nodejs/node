const common = require('../common');
const assert = require('assert');

process.stdout.write('hello world\r\n');

const stdin = process.openStdin();

stdin.on('data', (data) => {
  process.stdout.write(data.toString());
});
