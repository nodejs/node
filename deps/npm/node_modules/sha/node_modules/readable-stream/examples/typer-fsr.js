var fs = require('fs');
var FSReadable = require('../fs.js');
var rst = new FSReadable(__filename);

rst.on('end', function() {
  process.stdin.pause();
});

process.stdin.setRawMode(true);
process.stdin.on('data', function() {
  var c = rst.read(3);
  if (!c) return;
  process.stdout.write(c);
});
process.stdin.resume();
