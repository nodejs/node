'use strict';

var http = require('http');

var port = parseInt(process.env.PORT || 8000);

var fixed = 'C'.repeat(20 * 1024),
  storedBytes = {},
  storedBuffer = {},
  storedUnicode = {};

var useDomains = process.env.NODE_USE_DOMAINS;

// set up one global domain.
if (useDomains) {
  var domain = require('domain');
  var gdom = domain.create();
  gdom.on('error', function(er) {
    console.error('Error on global domain', er);
    throw er;
  });
  gdom.enter();
}

var server = module.exports = http.createServer(function(req, res) {
  if (useDomains) {
    var dom = domain.create();
    dom.add(req);
    dom.add(res);
  }

  var commands = req.url.split('/');
  var command = commands[1];
  var body = '';
  var arg = commands[2];
  var n_chunks = parseInt(commands[3], 10);
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
    const headers = {
      'Content-Type': 'text/plain',
      'Transfer-Encoding': 'chunked'
    };
    res.writeHead(200, headers);
    req.pipe(res);
    return;

  } else {
    status = 404;
    body = 'not found\n';
  }

  // example: http://localhost:port/bytes/512/4
  // sends a 512 byte body in 4 chunks of 128 bytes
  if (n_chunks > 0) {
    const headers = {
      'Content-Type': 'text/plain',
      'Transfer-Encoding': 'chunked'
    };
    res.writeHead(status, headers);
    // send body in chunks
    var len = body.length;
    var step = Math.floor(len / n_chunks) || 1;

    for (i = 0, n = (n_chunks - 1); i < n; ++i) {
      res.write(body.slice(i * step, i * step + step));
    }
    res.end(body.slice((n_chunks - 1) * step));
  } else {
    const headers = {
      'Content-Type': 'text/plain',
      'Content-Length': body.length.toString()
    };

    res.writeHead(status, headers);
    res.end(body);
  }
});

server.listen(port, function() {
  if (module === require.main)
    console.error('Listening at http://127.0.0.1:' + port + '/');
});
