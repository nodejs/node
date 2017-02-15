const common = require('../common');
const assert = require('assert');

process.stdout.write('hello world\r\n');

var stdin = process.openStdin();

stdin.on('data', function(data) {
  process.stdout.write(data.toString());
});
