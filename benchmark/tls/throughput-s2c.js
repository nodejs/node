'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  dur: [5],
  type: ['buf', 'asc', 'utf'],
  sendchunklen: [256, 32 * 1024, 128 * 1024, 16 * 1024 * 1024],
  recvbuflen: [0, 64 * 1024, 1024 * 1024],
  recvbufgenfn: ['true', 'false'],
});

const fixtures = require('../../test/common/fixtures');
let options;
let recvbuf;
let received = 0;
const tls = require('tls');

function main({ dur, type, sendchunklen, recvbuflen, recvbufgenfn }) {
  if (isFinite(recvbuflen) && recvbuflen > 0)
    recvbuf = Buffer.alloc(recvbuflen);

  let encoding;
  let chunk;
  switch (type) {
    case 'buf':
      chunk = Buffer.alloc(sendchunklen, 'b');
      break;
    case 'asc':
      chunk = 'a'.repeat(sendchunklen);
      encoding = 'ascii';
      break;
    case 'utf':
      chunk = 'ü'.repeat(sendchunklen / 2);
      encoding = 'utf8';
      break;
    default:
      throw new Error('invalid type');
  }

  options = {
    key: fixtures.readKey('rsa_private.pem'),
    cert: fixtures.readKey('rsa_cert.crt'),
    ca: fixtures.readKey('rsa_ca.crt'),
    ciphers: 'AES256-GCM-SHA384',
    maxVersion: 'TLSv1.2',
  };

  let socketOpts;
  if (recvbuf === undefined) {
    socketOpts = { port: common.PORT, rejectUnauthorized: false };
  } else {
    let buffer = recvbuf;
    if (recvbufgenfn === 'true') {
      let bufidx = -1;
      const bufpool = [
        recvbuf,
        Buffer.from(recvbuf),
        Buffer.from(recvbuf),
      ];
      buffer = () => {
        bufidx = (bufidx + 1) % bufpool.length;
        return bufpool[bufidx];
      };
    }
    socketOpts = {
      port: common.PORT,
      rejectUnauthorized: false,
      onread: {
        buffer,
        callback: function(nread, buf) {
          received += nread;
        },
      },
    };
  }

  const server = tls.createServer(options, (socket) => {
    socket.on('data', (buf) => {
      socket.on('drain', write);
      write();
    });

    function write() {
      while (false !== socket.write(chunk, encoding));
    }
  });

  let conn;
  server.listen(common.PORT, () => {
    conn = tls.connect(socketOpts, () => {
      setTimeout(done, dur * 1000);
      bench.start();
      conn.write('hello');
    });

    conn.on('data', (chunk) => {
      received += chunk.length;
    });
  });

  function done() {
    const mbits = (received * 8) / (1024 * 1024);
    bench.end(mbits);
    process.exit(0);
  }
}
