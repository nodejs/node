var common = require('../common');
var assert = require('assert');

common.print('hello world\r\n');

var stdin = process.openStdin();

stdin.addListener('data', function(data) {
  process.stdout.write(data.toString());
});

stdin.addListener('end', function() {
  process.stdout.end();
});
