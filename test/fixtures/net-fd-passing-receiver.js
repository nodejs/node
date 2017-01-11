process.mixin(require('../common'));
const net = require('net');

const path = process.ARGV[2];
const greeting = process.ARGV[3];

receiver = net.createServer((socket) => {
  socket.on('fd', (fd) => {
    const peerInfo = process.getpeername(fd);
    peerInfo.fd = fd;
    const passedSocket = new net.Socket(peerInfo);

    passedSocket.on('eof', () => {
      passedSocket.close();
    });

    passedSocket.on('data', (data) => {
      passedSocket.send(`[echo] ${data}`);
    });
    passedSocket.on('close', () => {
      receiver.close();
    });
    passedSocket.send(`[greeting] ${greeting}`);
  });
});

/* To signal the test runne we're up and listening */
receiver.on('listening', () => {
  console.log('ready');
});

receiver.listen(path);
