'use strict';
// Flags: --expose-gc

var common = require('../common');
var assert = require('assert');
var net = require('net');

assert.strictEqual(
  typeof global.gc,
  'function',
  'Run this test with --expose-gc'
);
net.createServer(function() {}).listen(common.PORT);

var before = 0;
{
  // 2**26 == 64M entries
  global.gc();
  let junk = [0];
  for (let i = 0; i < 26; ++i) junk = junk.concat(junk);
  before = process.memoryUsage().rss;

  net.createConnection(common.PORT, '127.0.0.1', function() {
    assert.notStrictEqual(junk.length, 0);  // keep reference alive
    setTimeout(done, 10);
    global.gc();
  });
}

function done() {
  global.gc();
  var after = process.memoryUsage().rss;
  var reclaimed = (before - after) / 1024;
  console.log('%d kB reclaimed', reclaimed);
  assert(reclaimed > 128 * 1024);  // It's around 256 MB on x64.
  process.exit();
}
