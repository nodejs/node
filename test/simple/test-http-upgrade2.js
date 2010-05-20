require('../common');

http = require('http');
net = require('net');

server = http.createServer(function (req, res) {
  error('got req');
  throw new Error("This shouldn't happen.");
});

server.addListener('upgrade', function (req, socket, upgradeHead) {
  error('got upgrade event');
  // test that throwing an error from upgrade gets 
  // is uncaught
  throw new Error('upgrade error');
});

gotError = false;

process.addListener('uncaughtException', function (e) {
  error('got "clientError" event');
  assert.equal('upgrade error', e.message);
  gotError = true;
  process.exit(0);
});


server.listen(PORT);


server.addListener('listening', function () {
  var c = net.createConnection(PORT);

  c.addListener('connect', function () {
    error('client wrote message');
    c.write( "GET /blah HTTP/1.1\r\n"
           + "Upgrade: WebSocket\r\n"
           + "Connection: Upgrade\r\n"
           + "\r\n\r\nhello world"
           );
  });

  c.addListener('end', function () {
    c.end();
  });

  c.addListener('close', function () {
    error('client close');
    server.close();
  });
});

process.addListener('exit', function () {
  assert.ok(gotError);
});
