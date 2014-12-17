// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');

if (!common.opensslCli) {
  console.error('Skipping because node compiled without OpenSSL CLI.');
  process.exit(0);
}

var assert = require('assert');

var join = require('path').join;
var net = require('net');
var fs = require('fs');
var tls = require('tls');
var spawn = require('child_process').spawn;

var connections = 0;
var key = fs.readFileSync(join(common.fixturesDir, 'agent.key')).toString();
var cert = fs.readFileSync(join(common.fixturesDir, 'agent.crt')).toString();

function log(a) {
  console.error('***server*** ' + a);
}

var server = net.createServer(function(socket) {
  connections++;
  log('connection fd=' + socket.fd);
  var sslcontext = tls.createSecureContext({key: key, cert: cert});
  sslcontext.context.setCiphers('RC4-SHA:AES128-SHA:AES256-SHA');

  var pair = tls.createSecurePair(sslcontext, true);

  assert.ok(pair.encrypted.writable);
  assert.ok(pair.cleartext.writable);

  pair.encrypted.pipe(socket);
  socket.pipe(pair.encrypted);

  log('i set it secure');

  pair.on('secure', function() {
    log('connected+secure!');
    pair.cleartext.write('hello\r\n');
    log(pair.cleartext.getPeerCertificate());
    log(pair.cleartext.getCipher());
  });

  pair.cleartext.on('data', function(data) {
    log('read bytes ' + data.length);
    pair.cleartext.write(data);
  });

  socket.on('end', function() {
    log('socket end');
  });

  pair.cleartext.on('error', function(err) {
    log('got error: ');
    log(err);
    log(err.stack);
    socket.destroy();
  });

  pair.encrypted.on('error', function(err) {
    log('encrypted error: ');
    log(err);
    log(err.stack);
    socket.destroy();
  });

  socket.on('error', function(err) {
    log('socket error: ');
    log(err);
    log(err.stack);
    socket.destroy();
  });

  socket.on('close', function(err) {
    log('socket closed');
  });

  pair.on('error', function(err) {
    log('secure error: ');
    log(err);
    log(err.stack);
    socket.destroy();
  });
});

var gotHello = false;
var sentWorld = false;
var gotWorld = false;
var opensslExitCode = -1;

server.listen(common.PORT, function() {
  // To test use: openssl s_client -connect localhost:8000
  var client = spawn(common.opensslCli, ['s_client', '-connect', '127.0.0.1:' +
        common.PORT]);


  var out = '';

  client.stdout.setEncoding('utf8');
  client.stdout.on('data', function(d) {
    out += d;

    if (!gotHello && /hello/.test(out)) {
      gotHello = true;
      client.stdin.write('world\r\n');
      sentWorld = true;
    }

    if (!gotWorld && /world/.test(out)) {
      gotWorld = true;
      client.stdin.end();
    }
  });

  client.stdout.pipe(process.stdout, { end: false });

  client.on('exit', function(code) {
    opensslExitCode = code;
    server.close();
  });
});

process.on('exit', function() {
  assert.equal(1, connections);
  assert.ok(gotHello);
  assert.ok(sentWorld);
  assert.ok(gotWorld);
  assert.equal(0, opensslExitCode);
});
