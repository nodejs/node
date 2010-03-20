require('../common');

var
  path = require('path'),
  fs = require('fs'),
  fn = path.join(fixturesDir, 'multipart.js'),
  file = fs.createReadStream(fn),

  callbacks = {
    open: -1,
    end: -1,
    close: -1,
    forceClose: -1
  },

  paused = false,

  fileContent = '';

file
  .addListener('open', function(fd) {
    callbacks.open++;
    assert.equal('number', typeof fd);
    assert.ok(file.readable);
  })
  .addListener('error', function(err) {
    throw err;
  })
  .addListener('data', function(data) {
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
  })
  .addListener('end', function(chunk) {
    callbacks.end++;
  })
  .addListener('close', function() {
    callbacks.close++;
    assert.ok(!file.readable);

    assert.equal(fs.readFileSync(fn), fileContent);
  });

var file2 = fs.createReadStream(fn);
file2.forceClose(function(err) {
  assert.ok(!err);
  callbacks.forceClose++;
});

process.addListener('exit', function() {
  for (var k in callbacks) {
    assert.equal(0, callbacks[k], k+' count off by '+callbacks[k]);
  }
});
