var fs = require('fs');
var fst = fs.createReadStream(__filename);
var Readable = require('../readable.js');
var rst = new Readable();
rst.wrap(fst);

rst.on('end', function() {
  process.stdin.pause();
});

process.stdin.setRawMode(true);
process.stdin.on('data', function() {
  var c = rst.read(3);
  if (!c) return setTimeout(process.exit, 500)
  process.stdout.write(c);
});
process.stdin.resume();
