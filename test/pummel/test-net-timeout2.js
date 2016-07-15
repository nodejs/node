'use strict';
// socket.write was not resetting the timeout timer. See
// https://github.com/joyent/node/issues/2002

var common = require('../common');
var net = require('net');

var seconds = 5;
var counter = 0;

var server = net.createServer(function(socket) {
  socket.setTimeout((seconds / 2) * 1000, common.fail);

  var interval = setInterval(function() {
    counter++;

    if (counter == seconds) {
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
  var s = net.connect(common.PORT);
  s.pipe(process.stdout);
});
