require('../common');

// TODO Improved this test. test_ca.pem is too small. A proper test would
// great a large utf8 (with multibyte chars) file and stream it in,
// performing sanity checks throughout.

Buffer = require('buffer').Buffer;
path = require('path');
fs = require('fs');
fn = path.join(fixturesDir, 'test_ca.pem');

file = fs.createReadStream(fn);

callbacks = {
  open: -1,
  end: -1,
  data: -1,
  close: -1,
  destroy: -1
};

paused = false;

fileContent = '';

file.addListener('open', function(fd) {
  callbacks.open++;
  assert.equal('number', typeof fd);
  assert.ok(file.readable);
});

file.addListener('error', function(err) {
  throw err;
});

file.addListener('data', function(data) {
  callbacks.data++;
  assert.ok(data instanceof Buffer);
  assert.ok(!paused);
  fileContent += data;
  
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

  assert.equal(fs.readFileSync(fn), fileContent);
});

var file2 = fs.createReadStream(fn);
file2.destroy(function(err) {
  assert.ok(!err);
  callbacks.destroy++;
});

process.addListener('exit', function() {
  for (var k in callbacks) {
    assert.equal(0, callbacks[k], k+' count off by '+callbacks[k]);
  }
});
