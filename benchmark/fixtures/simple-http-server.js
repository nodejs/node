'use strict';

const http = require('http');

const fixed = 'C'.repeat(20 * 1024);
const storedBytes = Object.create(null);
const storedBuffer = Object.create(null);
const storedUnicode = Object.create(null);

const useDomains = process.env.NODE_USE_DOMAINS;

// set up one global domain.
if (useDomains) {
  var domain = require('domain');
  const gdom = domain.create();
  gdom.on('error', (er) => {
    console.error('Error on global domain', er);
    throw er;
  });
  gdom.enter();
}

module.exports = http.createServer((req, res) => {
  if (useDomains) {
    const dom = domain.create();
    dom.add(req);
    dom.add(res);
  }

  // URL format: /<type>/<length>/<chunks>/<responseBehavior>/chunkedEnc
  const params = req.url.split('/');
  const command = params[1];
  var body = '';
  const arg = params[2];
  const n_chunks = parseInt(params[3], 10);
  const resHow = params.length >= 5 ? params[4] : 'normal';
  const chunkedEnc = params.length >= 6 && params[5] === '0' ? false : true;
  var status = 200;

  var n, i;
  if (command === 'bytes') {
    n = ~~arg;
    if (n <= 0)
      throw new Error('bytes called with n <= 0');
    if (storedBytes[n] === undefined) {
      storedBytes[n] = 'C'.repeat(n);
    }
    body = storedBytes[n];
  } else if (command === 'buffer') {
    n = ~~arg;
    if (n <= 0)
      throw new Error('buffer called with n <= 0');
    if (storedBuffer[n] === undefined) {
      storedBuffer[n] = Buffer.allocUnsafe(n);
      for (i = 0; i < n; i++) {
        storedBuffer[n][i] = 'C'.charCodeAt(0);
      }
    }
    body = storedBuffer[n];
  } else if (command === 'unicode') {
    n = ~~arg;
    if (n <= 0)
      throw new Error('unicode called with n <= 0');
    if (storedUnicode[n] === undefined) {
      storedUnicode[n] = '\u263A'.repeat(n);
    }
    body = storedUnicode[n];
  } else if (command === 'quit') {
    res.connection.server.close();
    body = 'quitting';
  } else if (command === 'fixed') {
    body = fixed;
  } else if (command === 'echo') {
    switch (resHow) {
      case 'setHeader':
        res.statusCode = 200;
        res.setHeader('Content-Type', 'text/plain');
        res.setHeader('Transfer-Encoding', 'chunked');
        break;
      case 'setHeaderWH':
        res.setHeader('Content-Type', 'text/plain');
        res.writeHead(200, { 'Transfer-Encoding': 'chunked' });
        break;
      default:
        res.writeHead(200, {
          'Content-Type': 'text/plain',
          'Transfer-Encoding': 'chunked'
        });
    }
    req.pipe(res);
    return;
  } else {
    status = 404;
    body = 'not found\n';
  }

  // example: http://localhost:port/bytes/512/4
  // sends a 512 byte body in 4 chunks of 128 bytes
  const len = body.length;
  switch (resHow) {
    case 'setHeader':
      res.statusCode = status;
      res.setHeader('Content-Type', 'text/plain');
      if (chunkedEnc)
        res.setHeader('Transfer-Encoding', 'chunked');
      else
        res.setHeader('Content-Length', len.toString());
      break;
    case 'setHeaderWH':
      res.setHeader('Content-Type', 'text/plain');
      if (chunkedEnc)
        res.writeHead(status, { 'Transfer-Encoding': 'chunked' });
      else
        res.writeHead(status, { 'Content-Length': len.toString() });
      break;
    default:
      if (chunkedEnc) {
        res.writeHead(status, {
          'Content-Type': 'text/plain',
          'Transfer-Encoding': 'chunked'
        });
      } else {
        res.writeHead(status, {
          'Content-Type': 'text/plain',
          'Content-Length': len.toString()
        });
      }
  }
  // send body in chunks
  if (n_chunks > 1) {
    const step = Math.floor(len / n_chunks) || 1;
    for (i = 0, n = (n_chunks - 1); i < n; ++i)
      res.write(body.slice(i * step, i * step + step));
    res.end(body.slice((n_chunks - 1) * step));
  } else {
    res.end(body);
  }
});
