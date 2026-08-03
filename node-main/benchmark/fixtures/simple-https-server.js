'use strict';

const fixtures = require('../../test/common/fixtures');
const https = require('https');

const options = {
  key: fixtures.readKey('rsa_private.pem'),
  cert: fixtures.readKey('rsa_cert.crt')
};

const storedBytes = Object.create(null);
const storedBuffer = Object.create(null);

module.exports = https.createServer(options, (req, res) => {
  // URL format: /<type>/<length>/<chunks>/chunkedEnc
  const params = req.url.split('/');
  const command = params[1];
  let body = '';
  const arg = params[2];
  const n_chunks = parseInt(params[3], 10);
  const chunkedEnc = params.length >= 5 && params[4] === '0' ? false : true;
  let status = 200;

  let n, i;
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
  } else {
    status = 404;
    body = 'not found\n';
  }

  // example: https://localhost:port/bytes/512/4
  // sends a 512 byte body in 4 chunks of 128 bytes
  const len = body.length;
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
