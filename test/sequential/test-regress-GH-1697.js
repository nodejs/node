'use strict';
const common = require('../common');
const net = require('net');
const cp = require('child_process');

if (process.argv[2] === 'server') {
  // Server

  var server = net.createServer(function(conn) {
    conn.on('data', function(data) {
      console.log('server received ' + data.length + ' bytes');
    });

    conn.on('close', function() {
      server.close();
    });
  });

  server.listen(common.PORT, '127.0.0.1', function() {
    console.log('Server running.');
  });

} else {
  // Client

  var serverProcess = cp.spawn(process.execPath, [process.argv[1], 'server']);
  serverProcess.stdout.pipe(process.stdout);
  serverProcess.stderr.pipe(process.stdout);

  serverProcess.stdout.once('data', function() {
    var client = net.createConnection(common.PORT, '127.0.0.1');
    client.on('connect', function() {
      const alot = Buffer.allocUnsafe(1024);
      const alittle = Buffer.allocUnsafe(1);

      for (var i = 0; i < 100; i++) {
        client.write(alot);
      }

      // Block the event loop for 1 second
      var start = (new Date()).getTime();
      while ((new Date()).getTime() < start + 1000) {}

      client.write(alittle);

      client.destroySoon();
    });
  });
}
