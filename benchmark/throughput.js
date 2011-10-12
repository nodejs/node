var fork = require('child_process').fork;
var net = require('net');
var buffer = new Buffer(1024 * 1024);

function write(socket) {
  if (!socket.writable) return;

  socket.write(buffer, function() {
    write(socket);
  });
}

var server = net.createServer(function(socket) {
  server.close();
  write(socket);
});

server.listen(8000, function() {
  fork(__dirname + '/throughput-child.js');
});

