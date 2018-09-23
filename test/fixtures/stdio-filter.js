var util = require('util');

var regexIn = process.argv[2];
var replacement = process.argv[3];
var re = new RegExp(regexIn, 'g');
var stdin = process.openStdin();

stdin.on('data', function(data) {
  data = data.toString();
  process.stdout.write(data.replace(re, replacement));
});
