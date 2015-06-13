'use strict';
var common = require('../common');
var assert = require('assert');

var path = require('path'),
  fs = require('fs'),
  filename = path.join(common.fixturesDir, 'large_file.txt');

var filesize = 1024 * 1024 * 1024;

function makeFile(done) {
  var buf = new Buffer(filesize / 1024);
  buf.fill('a');

  try { fs.unlinkSync(filename); } catch (e) {}
  var w = 1024;
  var ws = fs.createWriteStream(filename);
  ws.on('close', done);
  ws.on('drain', write);
  write();
  function write() {
    do {
      w--;
    } while (false !== ws.write(buf) && w > 0);
    if (w === 0)
      ws.end();
  }
}

makeFile(function() {
  fs.readFile(filename, function(err) {
    assert.ok(err, 'should get RangeError');
    assert.equal(err.name, 'RangeError', 'should get RangeError');
    try { fs.unlinkSync(filename); } catch (e) {}
  });
});

process.on('uncaughtException', function(err) {
  assert.ok(!err, 'should not throw uncaughtException');
});
