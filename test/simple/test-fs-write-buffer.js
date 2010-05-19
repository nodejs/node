require('../common');
var path = require('path')
  , Buffer = require('buffer').Buffer
  , fs = require('fs')
  , fn = path.join(fixturesDir, 'write.txt')
  , expected = new Buffer('hello')
  , openCalled = 0
  , writeCalled = 0;


fs.open(fn, 'w', 0644, function (err, fd) {
  openCalled++;
  if (err) throw err;

  fs.write(fd, expected, 0, expected.length, null, function(err, written) {
    writeCalled++;
    if (err) throw err;

    assert.equal(expected.length, written);
    fs.closeSync(fd);

    var found = fs.readFileSync(fn);
    assert.deepEqual(found, expected.toString());
    fs.unlinkSync(fn);
  });
});

process.addListener("exit", function () {
  assert.equal(openCalled, 1);
  assert.equal(writeCalled, 1);
});

