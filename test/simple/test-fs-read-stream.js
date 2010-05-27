require('../common');

// TODO Improved this test. test_ca.pem is too small. A proper test would
// great a large utf8 (with multibyte chars) file and stream it in,
// performing sanity checks throughout.

Buffer = require('buffer').Buffer;
path = require('path');
fs = require('fs');
fn = path.join(fixturesDir, 'elipses.txt');

callbacks = { open: 0, end: 0, close: 0, destroy: 0 };

paused = false;

file = fs.createReadStream(fn);

file.addListener('open', function(fd) {
  file.length = 0;
  callbacks.open++;
  assert.equal('number', typeof fd);
  assert.ok(file.readable);
});


file.addListener('data', function(data) {
  assert.ok(data instanceof Buffer);
  assert.ok(!paused);
  file.length += data.length;

  paused = true;
  file.pause();
  assert.ok(file.paused);

  setTimeout(function() {
    paused = false;
    file.resume();
    assert.ok(!file.paused);
  }, 10);
});


file.addListener('end', function(chunk) {
  callbacks.end++;
});


file.addListener('close', function() {
  callbacks.close++;
  assert.ok(!file.readable);

  //assert.equal(fs.readFileSync(fn), fileContent);
});

var file2 = fs.createReadStream(fn);
file2.destroy(function(err) {
  assert.ok(!err);
  callbacks.destroy++;
});

var file3 = fs.createReadStream(fn);
file3.length = 0;
file3.setEncoding('utf8');
file3.addListener('data', function(data) {
  assert.equal("string", typeof(data));
  file3.length += data.length;

  for (var i = 0; i < data.length; i++) {
    // http://www.fileformat.info/info/unicode/char/2026/index.htm
    assert.equal("\u2026", data[i]);
  }
});

file3.addListener('close', function () {
  callbacks.close++;
});

process.addListener('exit', function() {
  assert.equal(1, callbacks.open);
  assert.equal(1, callbacks.end);
  assert.equal(1, callbacks.destroy);

  assert.equal(2, callbacks.close);

  assert.equal(30000, file.length);
  assert.equal(10000, file3.length);
});
