'use strict';
// Flags: --expose-gc

var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

var fs = require('fs');

assert(typeof gc === 'function', 'Run this test with --expose-gc');

tls.createServer({
  cert: fs.readFileSync(common.fixturesDir + '/test_cert.pem'),
  key: fs.readFileSync(common.fixturesDir + '/test_key.pem')
}).listen(common.PORT);

(function() {
  // 2**26 == 64M entries
  for (var i = 0, junk = [0]; i < 26; ++i) junk = junk.concat(junk);

  var options = { rejectUnauthorized: false };
  tls.connect(common.PORT, '127.0.0.1', options, function() {
    assert(junk.length != 0);  // keep reference alive
    setTimeout(done, 10);
    gc();
  });
})();

function done() {
  var before = process.memoryUsage().rss;
  gc();
  var after = process.memoryUsage().rss;
  var reclaimed = (before - after) / 1024;
  console.log('%d kB reclaimed', reclaimed);
  assert(reclaimed > 256 * 1024);  // it's more like 512M on x64
  process.exit();
}
