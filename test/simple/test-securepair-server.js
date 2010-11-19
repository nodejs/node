var common = require('../common');

var join = require('path').join;
var net = require('net');
var assert = require('assert');
var fs = require('fs');
var crypto = require('crypto');
var spawn = require('child_process').spawn;

var connections = 0;
var key = fs.readFileSync(join(common.fixturesDir, "agent.key")).toString();
var cert = fs.readFileSync(join(common.fixturesDir, "agent.crt")).toString();

function log (a) {
  console.log('***server*** ' + a);
}

var server = net.createServer(function (socket) {
  connections++;
  log('connection');
  var sslcontext = crypto.createCredentials({key: key, cert: cert});
  sslcontext.context.setCiphers('RC4-SHA:AES128-SHA:AES256-SHA');

  var spair = crypto.createPair(sslcontext, true);

  spair.encrypted.pipe(socket);
  socket.pipe(spair.encrypted);

  var clear = spair.cleartext;

  log('i set it secure');

  spair.on('secure', function () {
    log('connected+secure!');
    clear.write(new Buffer('hello\r\n'));
    log(spair.getPeerCertificate());
    log(spair.getCipher());
  });

  clear.on('data', function (data) {
    log('read %d bytes', data.length);
    clear.write(data);
  });

  socket.on('end', function (err) {
    log('all done: '+ err);
    clear.write(new Buffer('goodbye\r\n'));
    clear.end();
  });

  clear.on('error', function(err) {
    log('got error: ');
    log(err);
    log(err.stack);
    socket.destroy();
  });

  spair.encrypted.on('error', function(err) {
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

  spair.on('error', function(err) {
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

server.listen(8000, function () {
  // To test use: openssl s_client -connect localhost:8000
  var client = spawn('openssl', ['s_client', '-connect', '127.0.0.1:8000']);


  var out = '';

  client.stdout.setEncoding('utf8');
  client.stdout.on('data', function (d) {
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

  client.stdout.pipe(process.stdout);

  client.on('exit', function (code) {
    opensslExitCode = code;
    server.close();
  });
});

process.on('exit', function () {
  assert.equal(1, connections);
  assert.ok(gotHello);
  assert.ok(sentWorld);
  assert.ok(gotWorld);
  assert.equal(0, opensslExitCode);
});
