'use strict';
var common = require('../common.js');
var bench = common.createBenchmark(main, {
  dur: [5],
  type: ['buf', 'asc', 'utf'],
  size: [2, 1024, 1024 * 1024]
});

var dur, type, encoding, size;
var server;

var path = require('path');
var fs = require('fs');
var cert_dir = path.resolve(__dirname, '../../test/fixtures');
var options;
var tls = require('tls');

function main(conf) {
  dur = +conf.dur;
  type = conf.type;
  size = +conf.size;

  var chunk;
  switch (type) {
    case 'buf':
      chunk = Buffer.alloc(size, 'b');
      break;
    case 'asc':
      chunk = 'a'.repeat(size);
      encoding = 'ascii';
      break;
    case 'utf':
      chunk = 'ü'.repeat(size / 2);
      encoding = 'utf8';
      break;
    default:
      throw new Error('invalid type');
  }

  options = {
    key: fs.readFileSync(`${cert_dir}/test_key.pem`),
    cert: fs.readFileSync(`${cert_dir}/test_cert.pem`),
    ca: [ fs.readFileSync(`${cert_dir}/test_ca.pem`) ],
    ciphers: 'AES256-GCM-SHA384'
  };

  server = tls.createServer(options, onConnection);
  setTimeout(done, dur * 1000);
  var conn;
  server.listen(common.PORT, function() {
    var opt = { port: common.PORT, rejectUnauthorized: false };
    conn = tls.connect(opt, function() {
      bench.start();
      conn.on('drain', write);
      write();
    });

    function write() {
      while (false !== conn.write(chunk, encoding));
    }
  });

  var received = 0;
  function onConnection(conn) {
    conn.on('data', function(chunk) {
      received += chunk.length;
    });
  }

  function done() {
    var mbits = (received * 8) / (1024 * 1024);
    bench.end(mbits);
    if (conn)
      conn.destroy();
    server.close();
  }
}
