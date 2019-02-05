'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  dur: [5],
  type: ['buf', 'asc', 'utf'],
  size: [2, 1024, 1024 * 1024]
});

const path = require('path');
const fs = require('fs');
const cert_dir = path.resolve(__dirname, '../../test/fixtures');
var options;
const tls = require('tls');

function main({ dur, type, size }) {
  var encoding;
  var server;
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
  var conn;
  server.listen(common.PORT, () => {
    const opt = { port: common.PORT, rejectUnauthorized: false };
    conn = tls.connect(opt, () => {
      setTimeout(done, dur * 1000);
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
    conn.on('data', (chunk) => {
      received += chunk.length;
    });
  }

  function done() {
    const mbits = (received * 8) / (1024 * 1024);
    bench.end(mbits);
    if (conn)
      conn.destroy();
    server.close();
  }
}
