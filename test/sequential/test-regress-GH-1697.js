'use strict';
var common = require('../common');
var net = require('net'),
    cp = require('child_process'),
    util = require('util');

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
      var alot = new Buffer(1024),
          alittle = new Buffer(1);

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
