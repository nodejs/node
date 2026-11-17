'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// A zero-length write on a TLS socket used to read the next TLS record
// synchronously. When that write came from JavaScript running inside the
// HTTP parser's `on_body` callback, the parser was executed re-entrantly and
// the request body included memory that the client never sent.
//
// Request 1 is a HEAD request whose response headers are flushed early, so
// `res.end('done')` writes '' to the socket. Its 'end' handler runs from the
// tick queue while the body of request 2 is being parsed, with the next TLS
// record already received.

const assert = require('assert');
const https = require('https');
const net = require('net');
const tls = require('tls');
const { Duplex } = require('stream');
const fixtures = require('../common/fixtures');

// Holds back writes so that several TLS records reach the server in a single
// TCP write.
class Batch extends Duplex {
  constructor(socket) {
    super();
    this.socket = socket;
    this.hold = false;
    this.queue = [];
    socket.on('data', (chunk) => this.push(chunk));
    socket.on('end', () => this.push(null));
    socket.on('close', () => this.destroy());
  }
  _read() {}
  _write(chunk, encoding, callback) {
    if (this.hold)
      this.queue.push(chunk);
    else
      this.socket.write(chunk);
    callback();
  }
  _final(callback) {
    this.socket.end(callback);
  }
  _destroy(err, callback) {
    this.socket.destroy();
    callback(err);
  }
  flush() {
    this.hold = false;
    this.socket.write(Buffer.concat(this.queue));
    this.queue = [];
  }
}

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
};

// Sends request 1, then the rest of it plus the start of request 2 and
// `record` as two TLS records in one TCP write, then `tail`.
function test(record, tail, check) {
  return new Promise((resolve) => {
    const bodies = [];
    const server = https.createServer(options, (req, res) => {
      res.flushHeaders();
      const chunks = [];
      bodies.push(chunks);
      req.on('data', (chunk) => chunks.push(chunk));
      req.on('error', () => {});
      req.on('end', () => res.end('done'));
    });

    server.listen(0, common.mustCall(() => {
      const batch = new Batch(net.connect(server.address().port));
      const socket = tls.connect({ socket: batch, rejectUnauthorized: false },
                                 common.mustCall(() => {
                                   socket.write('HEAD / HTTP/1.1\r\nHost: x\r\n' +
                                                'Transfer-Encoding: chunked\r\n\r\n');
                                 }));
      let response = '';
      socket.setEncoding('utf8');
      socket.on('data', (chunk) => {
        response += chunk;
        if (response.endsWith('\r\n4\r\ndone\r\n0\r\n\r\n'))
          socket.end();
      });
      socket.on('error', () => {});
      socket.once('data', common.mustCall(() => {
        batch.hold = true;
        socket.write('0\r\n\r\nPOST / HTTP/1.1\r\nHost: x\r\n' +
                     'Transfer-Encoding: chunked\r\n\r\n1\r\nZ', common.mustCall(() => {
          socket.write(record, common.mustCall(() => {
            batch.flush();
            if (tail)
              socket.write(tail);
          }));
        }));
      }));
      socket.on('close', common.mustCall(() => {
        server.close();
        check(bodies.map((chunks) => Buffer.concat(chunks).toString('latin1')),
              response);
        resolve();
      }));
    }));
  });
}

(async () => {
  // A valid request 2: its body arrives intact and both requests get a
  // response.
  await test('\r\n5\r\nHE', 'LLO\r\n0\r\n\r\n', (bodies, response) => {
    assert.deepStrictEqual(bodies, ['', 'ZHELLO']);
    assert.strictEqual(response.split('HTTP/1.1 200 OK\r\n').length, 3);
    assert.strictEqual(response.split('done').length, 2);
  });

  // The chunk 'Z' is not followed by CRLF, so request 2 is a parse error. The
  // body must not contain anything but the byte that was sent.
  await test('5\r\nHE', null, (bodies, response) => {
    assert.deepStrictEqual(bodies, ['', 'Z']);
    assert.match(response, /^HTTP\/1\.1 200 OK\r\n/);
  });
})().then(common.mustCall());
