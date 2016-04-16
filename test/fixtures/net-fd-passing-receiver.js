process.mixin(require('../common'));
net = require('net');

path = process.ARGV[2];
greeting = process.ARGV[3];

receiver = net.createServer(function(socket) {
  socket.on('fd', function(fd) {
    var peerInfo = process.getpeername(fd);
    peerInfo.fd = fd;
    var passedSocket = new net.Socket(peerInfo);

    passedSocket.on('eof', function() {
      passedSocket.close();
    });

    passedSocket.on('data', function(data) {
      passedSocket.send('[echo] ' + data);
    });
    passedSocket.on('close', function() {
      receiver.close();
    });
    passedSocket.send('[greeting] ' + greeting);
  });
});

/* To signal the test runne we're up and listening */
receiver.on('listening', function() {
  console.log('ready');
});

receiver.listen(path);
