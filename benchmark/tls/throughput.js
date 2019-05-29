'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  dur: [5],
  type: ['buf', 'asc', 'utf'],
  size: [2, 1024, 1024 * 1024]
});

const fixtures = require('../../test/common/fixtures');
var options;
const tls = require('tls');

function main({ dur, type, size }) {
  var encoding;
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
    key: fixtures.readKey('rsa_private.pem'),
    cert: fixtures.readKey('rsa_cert.crt'),
    ca: fixtures.readKey('rsa_ca.crt'),
    ciphers: 'AES256-GCM-SHA384'
  };

  const server = tls.createServer(options, onConnection);
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
