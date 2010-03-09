require("../common");
var path = require('path');
var fs = require('fs');
var fn = path.join(fixturesDir, "write.txt");
var expected = "hello";
var found;

fs.open(fn, 'w', 0644, function (err, fd) {
  if (err) throw err;
  puts('open done');
  fs.write(fd, expected, 0, "utf8", function (err, written) {
    puts('write done');
    if (err) throw err;
    assert.equal(expected.length, written);
    fs.closeSync(fd);
    found = fs.readFileSync(fn, 'utf8');
    puts('expected: ' + expected.toJSON());
    puts('found: ' + found.toJSON());
    fs.unlinkSync(fn);
  });
});

process.addListener("exit", function () {
  assert.equal(expected, found);
});

