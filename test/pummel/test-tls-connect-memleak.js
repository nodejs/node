'use strict';
// Flags: --expose-gc

var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');

assert(typeof global.gc === 'function', 'Run this test with --expose-gc');

tls.createServer({
  cert: fs.readFileSync(common.fixturesDir + '/test_cert.pem'),
  key: fs.readFileSync(common.fixturesDir + '/test_key.pem')
}).listen(common.PORT);

{
  // 2**26 == 64M entries
  let junk = [0];

  for (let i = 0; i < 26; ++i) junk = junk.concat(junk);

  const options = { rejectUnauthorized: false };
  tls.connect(common.PORT, '127.0.0.1', options, function() {
    assert(junk.length != 0);  // keep reference alive
    setTimeout(done, 10);
    global.gc();
  });
}

function done() {
  var before = process.memoryUsage().rss;
  global.gc();
  var after = process.memoryUsage().rss;
  var reclaimed = (before - after) / 1024;
  console.log('%d kB reclaimed', reclaimed);
  assert(reclaimed > 256 * 1024);  // it's more like 512M on x64
  process.exit();
}
