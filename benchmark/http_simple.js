var path = require('path'),
    exec = require('child_process').exec,
    http = require('http');

var port = parseInt(process.env.PORT || 8000);

console.log('pid ' + process.pid);

var fixed = makeString(20 * 1024, 'C'),
    storedBytes = {},
    storedBuffer = {};

var useDomains = process.env.NODE_USE_DOMAINS;

// set up one global domain.
if (useDomains) {
  var domain = require('domain');
  var gdom = domain.create();
  gdom.on('error', function(er) {
    console.log('Error on global domain', er);
    throw er;
  });
  gdom.enter();
}

var server = http.createServer(function (req, res) {
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

  if (command == 'bytes') {
    var n = ~~arg;
    if (n <= 0)
      throw new Error('bytes called with n <= 0')
    if (storedBytes[n] === undefined) {
      console.log('create storedBytes[n]');
      storedBytes[n] = makeString(n, 'C');
    }
    body = storedBytes[n];

  } else if (command == 'buffer') {
    var n = ~~arg;
    if (n <= 0)
      throw new Error('buffer called with n <= 0');
    if (storedBuffer[n] === undefined) {
      console.log('create storedBuffer[n]');
      storedBuffer[n] = new Buffer(n);
      for (var i = 0; i < n; i++) {
        storedBuffer[n][i] = 'C'.charCodeAt(0);
      }
    }
    body = storedBuffer[n];

  } else if (command == 'quit') {
    res.connection.server.close();
    body = 'quitting';

  } else if (command == 'fixed') {
    body = fixed;

  } else if (command == 'echo') {
    res.writeHead(200, { 'Content-Type': 'text/plain',
                         'Transfer-Encoding': 'chunked' });
    req.pipe(res);
    return;

  } else {
    status = 404;
    body = 'not found\n';
  }

  // example: http://localhost:port/bytes/512/4
  // sends a 512 byte body in 4 chunks of 128 bytes
  if (n_chunks > 0) {
    res.writeHead(status, { 'Content-Type': 'text/plain',
                            'Transfer-Encoding': 'chunked' });
    // send body in chunks
    var len = body.length;
    var step = ~~(len / n_chunks) || len;

    for (var i = 0; i < len; i += step) {
      res.write(body.slice(i, i + step));
    }

    res.end();
  } else {
    var content_length = body.length.toString();

    res.writeHead(status, { 'Content-Type': 'text/plain',
                            'Content-Length': content_length });
    res.end(body);
  }
});

function makeString(size, c) {
  var s = '';
  while (s.length < size) {
    s += c;
  }
  return s;
}

server.listen(port, function () {
  console.log('Listening at http://127.0.0.1:'+port+'/');
});

process.on('exit', function() {
  console.error('libuv counters', process.uvCounters());
});
