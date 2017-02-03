'use strict';
// socket.write was not resetting the timeout timer. See
// https://github.com/joyent/node/issues/2002

const common = require('../common');
const net = require('net');

const seconds = 5;
let counter = 0;

const server = net.createServer(function(socket) {
  socket.setTimeout((seconds / 2) * 1000, common.mustNotCall());

  const interval = setInterval(function() {
    counter++;

    if (counter === seconds) {
      clearInterval(interval);
      server.close();
      socket.destroy();
    }

    if (socket.writable) {
      socket.write(Date.now() + '\n');
    }
  }, 1000);
});


server.listen(common.PORT, function() {
  const s = net.connect(common.PORT);
  s.pipe(process.stdout);
});
