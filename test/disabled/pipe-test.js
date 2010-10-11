var common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var listenCount = 0;
var gotThanks = false;
var tcpLengthSeen;


/*
 * 5MB of random buffer.
 */
var buffer = Buffer(1024 * 1024 * 5);
for (var i = 0; i < buffer.length; i++) {
  buffer[i] = parseInt(Math.random()*10000);
}


var web = http.Server(function (req, res) {
  web.close();

  console.log(req.headers);

  var socket = net.Stream();
  socket.connect(tcpPort);

  req.pipe(socket);

  req.on('end', function () {
    res.writeHead(200);
    res.write("thanks");
    res.end();
  });
});
var webPort = common.PORT
web.listen(webPort, startClient);



var tcp = net.Server(function (socket) {
  tcp.close();

  var i = 0;

  socket.on('data', function (d) {
    for (var j = 0; j < d.length; j++) {
      assert.equal(i % 256, d[i]);
      i++;
    }
  });

  socket.on('end', function () {
    tcpLengthSeen = i;
    socket.end();
  });
});
var tcpPort = webPort + 1;
tcp.listen(tcpPort, startClient);


function startClient () {
  listenCount++;
  console.log("listenCount %d" , listenCount);
  if (listenCount < 2) {
    console.log("dont start client %d" , listenCount);
    return;
  }

  console.log("start client");

  var client = http.createClient(common.PORT);
  var req = client.request('GET', '/');
  req.write(buffer);
  req.end();

  req.on('response', function (res) {
    res.setEncoding('utf8');
    res.on('data', function (s) {
      assert.equal("thanks", s);
      gotThanks = true;
    });
  });
}

process.on('exit', function () {
  assert.ok(gotThanks);
  assert.equal(1024*1024*5, tcpLengthSeen);
});

