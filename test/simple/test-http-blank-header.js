common = require("../common");
assert = common.assert

http = require('http');
net = require('net');

gotReq = false;

server = http.createServer(function (req, res) {
  common.error('got req');
  gotReq = true;
  assert.equal('GET', req.method);
  assert.equal('/blah', req.url);
  assert.deepEqual({
    host: "mapdevel.trolologames.ru:443",
    origin: "http://mapdevel.trolologames.ru",
    cookie: "",
  }, req.headers);
});


server.listen(common.PORT, function () {
  var c = net.createConnection(common.PORT);

  c.addListener('connect', function () {
    common.error('client wrote message');
    c.write( "GET /blah HTTP/1.1\r\n"
           + "Host: mapdevel.trolologames.ru:443\r\n"
           + "Cookie:\r\n"
           + "Origin: http://mapdevel.trolologames.ru\r\n"
           + "\r\n\r\nhello world"
           );
  });

  c.addListener('end', function () {
    c.end();
  });

  c.addListener('close', function () {
    common.error('client close');
    server.close();
  });
});


process.addListener('exit', function () {
  assert.ok(gotReq);
});
