common = require("../common");
assert = common.assert
var path = require('path');
var Buffer = require('buffer').Buffer;
var fs = require('fs');
var fn = path.join(common.fixturesDir, "write.txt");
var expected = "ümlaut.";
var found;

fs.open(fn, 'w', 0644, function (err, fd) {
  if (err) throw err;
  console.log('open done');
  fs.write(fd, expected, 0, "utf8", function (err, written) {
    console.log('write done');
    if (err) throw err;
    assert.equal(Buffer.byteLength(expected), written);
    fs.closeSync(fd);
    found = fs.readFileSync(fn, 'utf8');
    console.log('expected: ' + expected.toJSON());
    console.log('found: ' + found.toJSON());
    fs.unlinkSync(fn);
  });
});

process.addListener("exit", function () {
  assert.equal(expected, found);
});

